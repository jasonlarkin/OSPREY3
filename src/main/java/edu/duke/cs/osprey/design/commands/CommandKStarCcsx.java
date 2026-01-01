package edu.duke.cs.osprey.design.commands;

import com.beust.jcommander.JCommander;
import com.beust.jcommander.Parameter;
import com.beust.jcommander.Parameters;
import edu.duke.cs.osprey.astar.conf.ConfAStarTree;
import edu.duke.cs.osprey.confspace.SeqSpace;
import edu.duke.cs.osprey.confspace.Sequence;
import edu.duke.cs.osprey.confspace.compiled.ConfSpace;
import edu.duke.cs.osprey.ematrix.EnergyMatrix;
import edu.duke.cs.osprey.ematrix.SimpleReferenceEnergies;
import edu.duke.cs.osprey.ematrix.compiled.EmatCalculator;
import edu.duke.cs.osprey.ematrix.compiled.ErefCalculator;
import edu.duke.cs.osprey.energy.compiled.ConfEnergyCalculatorAdapter;
import edu.duke.cs.osprey.kstar.KStar;
import edu.duke.cs.osprey.kstar.KStarScoreWriter;
import edu.duke.cs.osprey.kstar.pfunc.GradientDescentPfunc;
import edu.duke.cs.osprey.parallelism.Parallelism;

import java.io.File;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;

import static edu.duke.cs.osprey.design.Main.Failure;
import static edu.duke.cs.osprey.design.Main.Success;

/**
 * Compatibility command for CCS-based K* runs.
 *
 * Historically, some CCKStar workflows invoke `osprey3 kstar --complex-confspace ... --target-confspace ... --design-confspace ...`.
 * This repo's newer CLI uses design YAML for K* (command: affinity), but CCKStar produces compiled conf spaces (.ccsx).
 *
 * This command loads compiled conf spaces and runs K* directly in the JVM (no JPype).
 */
@Parameters(commandDescription = CommandKStarCcsx.CommandDescription)
public class CommandKStarCcsx extends RunnableCommand {

	public static final String CommandName = "kstar";
	public static final String CommandDescription = "Run K* from compiled conf spaces (.ccsx) (CCKStar compatibility command).";

	@Parameter(names = "--complex-confspace", required = true, description = "Path to compiled complex conf space (.ccsx)")
	public String complexCcsx;

	@Parameter(names = "--target-confspace", required = true, description = "Path to compiled target/ligand conf space (.ccsx)")
	public String targetCcsx;

	@Parameter(names = "--design-confspace", required = true, description = "Path to compiled design/protein conf space (.ccsx)")
	public String designCcsx;

	@Parameter(names = "--results-file", description = "Write scored sequences to this TSV file")
	public String resultsFile = "kstar.results.tsv";

	@Parameter(names = "--cpu-cores", description = "CPU cores for task parallelism")
	public int cpuCores = Math.max(1, Runtime.getRuntime().availableProcessors());

	@Parameter(names = "--show-pfunc-progress", description = "If set, print progress during partition function calculation")
	public boolean showPfuncProgress = false;

	@Override
	public int run(JCommander commander, String[] args) {

		// Match other commands' help semantics, but do NOT require a design YAML.
		if (args.length == 1) {
			printHelp(commander);
			return Failure;
		}
		if (delegate.help) {
			printHelp(commander);
			return Success;
		}

		try {
			return runKStar();
		} catch (Exception ex) {
			System.err.println("ERROR: " + ex.getMessage());
			ex.printStackTrace();
			return Failure;
		}
	}

	private int runKStar() throws IOException {

		var protein = loadCcsx(designCcsx);
		var ligand = loadCcsx(targetCcsx);
		var complex = loadCcsx(complexCcsx);

		var parallelism = new Parallelism(cpuCores, 0, 0);
		var tasks = parallelism.makeTaskExecutor();

		// NOTE: --epsilon and --max-simultaneous-mutations are already defined on DesignFileDelegate.
		// Reuse those flags here to avoid duplicate JCommander options.
		var settings = new KStar.Settings.Builder()
			.setEpsilon(delegate.epsilon > 0 ? delegate.epsilon : 0.99)
			.setMaxSimultaneousMutations(delegate.maxSimultaneousMutations)
			.setShowPfuncProgress(showPfuncProgress)
			.addScoreConsoleWriter(new KStarScoreWriter.Formatter.Log())
			.addScoreFileWriter(new File(resultsFile), new KStarScoreWriter.Formatter.Log())
			.build();

		var kstar = new KStar(protein, ligand, complex, settings);

		for (var info : kstar.confSpaceInfos()) {

			var ccs = (ConfSpace)info.confSpace;

			// compiled conf energy calculator (uses native ConfEcalc when on CPU)
			var compiledEcalc = edu.duke.cs.osprey.energy.compiled.ConfEnergyCalculator.makeBest(ccs, parallelism);

			// reference energies
			SimpleReferenceEnergies eref = new ErefCalculator.Builder(compiledEcalc)
				.build()
				.calc(tasks);

			// energy matrix with caching (matches python runner file names)
			var ematPath = new File(String.format("emat.%s.dat", info.id));
			EnergyMatrix emat = new EmatCalculator.Builder(compiledEcalc)
				.setReferenceEnergies(eref)
				.setCacheFile(ematPath)
				.build()
				.calc(tasks);

			// adapt new compiled ecalc to old confEcalc API for K* pfunc implementations
			info.confEcalc = new ConfEnergyCalculatorAdapter.Builder(compiledEcalc, tasks)
				.setReferenceEnergies(eref)
				.build();

			info.pfuncFactory = (rcs) -> new GradientDescentPfunc(
				info.confEcalc,
				new ConfAStarTree.Builder(emat, rcs).setTraditional().build(),
				new ConfAStarTree.Builder(emat, rcs).setTraditional().build(),
				rcs.getNumConformations()
			);
		}

		// Some CCKStar/MONTAGE confspaces have an empty SeqSpace (no wild-type annotation and no mutations),
		// which makes KStar.run() crash because it always assumes sequences.get(0) exists.
		// In that case, score a single "default" fully-assigned sequence instead.
		SeqSpace seqSpace = complex.seqSpace();
		boolean hasWildType = seqSpace.containsWildTypeSequence();
		var mutants = seqSpace.getMutants(delegate.maxSimultaneousMutations, true);

		// Some compiled confspaces have:
		// - no wild-type sequence annotation
		// - no enumeratable mutants (e.g., positions present but no defined WT)
		// In that case, KStar.run() will build an empty `sequences` list and crash.
		if (!hasWildType && mutants.isEmpty()) {
			Sequence seq = new Sequence(seqSpace);
			for (SeqSpace.Position pos : seqSpace.positions) {
				if (pos.wildType != null) {
					seq.set(pos, pos.wildType);
				} else if (!pos.resTypes.isEmpty()) {
					seq.set(pos, pos.resTypes.get(0));
				} else {
					throw new IllegalStateException("SeqSpace position has no residue types: " + pos);
				}
			}
			System.out.println("computing K* scores for 1 default sequence (SeqSpace has no wild type and no mutations) ...");
			settings.scoreWriters.writeHeader();
			var scored = kstar.score(seq, tasks);
			settings.scoreWriters.writeScore(new KStarScoreWriter.ScoreInfo(0, 1, scored.sequence, scored.score, kstar));
		} else {
			// run K* for wild type + mutants (if any)
			kstar.run(tasks);
		}

		return Success;
	}

	private static ConfSpace loadCcsx(String path) throws IOException {
		byte[] bytes = Files.readAllBytes(Path.of(path));
		return ConfSpace.fromBytes(bytes);
	}

	@Override
	public String getCommandName() {
		return CommandName;
	}

	@Override
	public String getCommandDescription() {
		return CommandDescription;
	}
}


