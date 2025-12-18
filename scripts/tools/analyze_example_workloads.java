import java.io.File;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.*;
import java.util.stream.Collectors;
import java.util.stream.Stream;

/**
 * Analyze example workloads to determine:
 * - Number of atoms
 * - Number of atom pairs (Amber + EEF1)
 * - Distribution of workload sizes
 * 
 * This script can be run to inspect examples or execute them to get actual workload sizes.
 */
public class AnalyzeExampleWorkloads {
    
    public static void main(String[] args) {
        String examplesDir = args.length > 0 ? args[0] : "examples";
        
        System.out.println("=== OSPREY Example Workload Analysis ===\n");
        System.out.println("Analyzing examples in: " + new File(examplesDir).getAbsolutePath());
        System.out.println();
        
        // Find all example directories
        List<ExampleInfo> examples = findExamples(examplesDir);
        
        System.out.println("Found " + examples.size() + " example directories:\n");
        
        // Group by type and analyze
        Map<String, List<ExampleInfo>> byType = examples.stream()
            .collect(Collectors.groupingBy(ExampleInfo::getType));
        
        System.out.println("=== Summary by Type ===");
        for (Map.Entry<String, List<ExampleInfo>> entry : byType.entrySet()) {
            System.out.println(entry.getKey() + ": " + entry.getValue().size() + " examples");
        }
        System.out.println();
        
        // Print detailed list
        System.out.println("=== Detailed Example List ===");
        System.out.printf("%-30s %-15s %-20s %s%n", "Example", "Type", "Config Files", "PDB Files");
        System.out.println(String.join("", Collections.nCopies(100, "-")));
        
        for (ExampleInfo ex : examples) {
            System.out.printf("%-30s %-15s %-20s %s%n",
                ex.name,
                ex.type,
                ex.configFiles.size(),
                ex.pdbFiles.size()
            );
        }
        System.out.println();
        
        // Instructions for execution-based analysis
        System.out.println("=== Next Steps ===");
        System.out.println("To get actual workload sizes (atoms, pairs), you need to:");
        System.out.println("1. Load each example's ConfSpace");
        System.out.println("2. Create a NativeConfEnergyCalculator");
        System.out.println("3. Call calcEnergy() with a test conformation");
        System.out.println("4. Extract atom/pair counts from the C++ side");
        System.out.println();
        System.out.println("Or use the C++ benchmark with different test data sizes.");
    }
    
    private static List<ExampleInfo> findExamples(String examplesDir) {
        List<ExampleInfo> examples = new ArrayList<>();
        
        try {
            Path examplesPath = Paths.get(examplesDir);
            if (!Files.exists(examplesPath)) {
                System.err.println("Examples directory not found: " + examplesPath.toAbsolutePath());
                return examples;
            }
            
            Files.list(examplesPath)
                .filter(Files::isDirectory)
                .forEach(dir -> {
                    String name = dir.getFileName().toString();
                    ExampleInfo info = new ExampleInfo(name, dir);
                    if (info.hasConfigFiles() || info.hasPdbFiles()) {
                        examples.add(info);
                    }
                });
            
            examples.sort(Comparator.comparing(e -> e.name));
            
        } catch (Exception e) {
            System.err.println("Error scanning examples: " + e.getMessage());
            e.printStackTrace();
        }
        
        return examples;
    }
    
    static class ExampleInfo {
        String name;
        Path path;
        List<String> configFiles = new ArrayList<>();
        List<String> pdbFiles = new ArrayList<>();
        
        ExampleInfo(String name, Path path) {
            this.name = name;
            this.path = path;
            scanFiles();
        }
        
        void scanFiles() {
            try {
                Files.walk(path, 2)  // Max depth 2
                    .filter(Files::isRegularFile)
                    .forEach(file -> {
                        String filename = file.getFileName().toString().toLowerCase();
                        if (filename.endsWith(".cfg") || filename.endsWith(".conf")) {
                            configFiles.add(file.getFileName().toString());
                        } else if (filename.endsWith(".pdb")) {
                            pdbFiles.add(file.getFileName().toString());
                        }
                    });
            } catch (Exception e) {
                // Ignore
            }
        }
        
        boolean hasConfigFiles() {
            return !configFiles.isEmpty();
        }
        
        boolean hasPdbFiles() {
            return !pdbFiles.isEmpty();
        }
        
        String getType() {
            if (name.contains("junit")) return "junit";
            if (name.contains("python")) return "python";
            if (name.contains("gpu")) return "gpu";
            if (name.contains("ccs")) return "ccs";
            if (name.startsWith("1CC8")) return "1CC8";
            if (name.startsWith("2RL0")) return "2RL0";
            if (name.startsWith("1DG9")) return "1DG9";
            if (name.startsWith("1FSV")) return "1FSV";
            if (name.startsWith("4HEM")) return "4HEM";
            if (name.startsWith("4NPD")) return "4NPD";
            return "other";
        }
    }
}

