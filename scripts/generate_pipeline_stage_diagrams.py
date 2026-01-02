#!/usr/bin/env python3
"""
Generate per-stage “mnemonic” diagrams for the OSPREY pipeline.

These diagrams are meant to be referenced from:
`docs/performance/OSPREY_PIPELINE_PARALLELISM_ANALYSIS.md`

Requires:
  - python package: diagrams
  - system dependency: graphviz (dot)
"""

from __future__ import annotations

import os

from diagrams import Cluster, Diagram, Edge
from diagrams.onprem.client import Client
from diagrams.onprem.compute import Server
from diagrams.onprem.workflow import Airflow
from diagrams.generic.storage import Storage
from diagrams.programming.language import Cpp, Java, Python


OUT_DIR = "docs/performance/images"


def _graph_attr() -> dict[str, str]:
    return {
        "bgcolor": "white",
        "dpi": "300",
        "pad": "0.5",
        "fontsize": "18",
    }


# -----------------------
# “Language layer” views
# -----------------------


def generate_layer_energy_calc():
    """C++/JNA energy evaluation layer (ConfEcalc)."""
    with Diagram(
        "Layer: Energy Calculation (C++/JNA)",
        filename=f"{OUT_DIR}/layer_energy_calc",
        outformat="png",
        direction="LR",
        graph_attr=_graph_attr(),
        show=False,
    ):
        with Cluster("Java (OSPREY core)"):
            java_ecalc = Java("EnergyCalculator\n(calcSingle)")
            jna = Java("JNA bridge")

        with Cluster("C++ (ConfEcalc)"):
            ff = Cpp("Forcefield\n(Amber/EEF1)")
            ccd = Cpp("CCD minimization\n(OpenMP)")
            native_mem = Storage("Native buffers\n(JNA direct mem)")

        with Cluster("Parallelism knobs"):
            omp = Server("OpenMP threads\n(OSPREY_MINIMIZE_CCD_OMP)")

        java_ecalc >> Edge(label="call") >> jna
        jna >> Edge(label="native call", color="orange", style="dashed") >> ff
        ff >> Edge(label="minimize") >> ccd
        ccd >> Edge(label="alloc/read", style="dashed", color="red") >> native_mem
        native_mem >> Edge(label="energy", style="dashed") >> jna
        ccd >> Edge(label="parallel", color="green") >> omp


def generate_layer_java_energy_calc():
    """Java energy-matrix computation + task executor layer."""
    with Diagram(
        "Layer: Java Energy Matrix + Task Executor",
        filename=f"{OUT_DIR}/layer_java_energy_calc",
        outformat="png",
        direction="LR",
        graph_attr=_graph_attr(),
        show=False,
    ):
        with Cluster("Java"):
            ecalc = Java("EnergyCalculator")
            tasks = Java("TaskExecutor\n(ForkJoinPool)")
            emat = Java("EnergyMatrix\n(one-body/two-body)")

        with Cluster("Work items"):
            w1 = Server("calcSingle(pos,rc)")
            w2 = Server("calcPair(pos1,rc1,pos2,rc2)")
            w3 = Server("… many tasks …")

        ecalc >> Edge(label="submit", color="green") >> tasks
        tasks >> Edge(label="parallel tasks", color="green") >> w1
        tasks >> Edge(label="parallel tasks", color="green") >> w2
        tasks >> Edge(label="parallel tasks", color="green") >> w3

        w1 >> Edge(label="setOneBody", style="dashed") >> emat
        w2 >> Edge(label="setPair", style="dashed") >> emat
        w3 >> Edge(label="fill", style="dashed") >> emat


def generate_layer_kstar():
    """Java K* algorithm layer (A* trees + partition functions)."""
    with Diagram(
        "Layer: K* Algorithm (Java)",
        filename=f"{OUT_DIR}/layer_kstar_algorithm",
        outformat="png",
        direction="LR",
        graph_attr=_graph_attr(),
        show=False,
    ):
        with Cluster("Inputs"):
            confspace = Java("ConfSpace\n(.ccsx)")
            emat = Java("EnergyMatrix\n(cached)")
            confdb = Storage("ConfDB\n(optional)")

        with Cluster("K* core"):
            kstar = Java("K*")
            astar = Java("A* search trees")
            pfunc = Java("Partition\nfunctions")
            bigdec = Java("BigDecimal\n(high precision)")

        with Cluster("Outputs"):
            scores = Storage("Scored sequences\n(K* scores)")
            ensembles = Storage("Ensembles\n(PDB/TSV)")

        confspace >> Edge(label="defines") >> kstar
        emat >> Edge(label="energies") >> kstar
        confdb >> Edge(label="store/reuse", style="dashed", color="red") >> kstar

        kstar >> astar >> pfunc >> Edge(label="score") >> scores
        kstar >> Edge(label="heavy arithmetic", style="dashed") >> bigdec
        scores >> Edge(label="write", style="dashed") >> ensembles


def generate_layer_python_workflow():
    """Python orchestration layer (CCKStar) + subprocess tools."""
    with Diagram(
        "Layer: Python Workflow Orchestration (CCKStar)",
        filename=f"{OUT_DIR}/layer_python_workflow",
        outformat="png",
        direction="LR",
        graph_attr=_graph_attr(),
        show=False,
    ):
        with Cluster("Python (CCKStar)"):
            scope = Python("SCOPE\n(convex hulls)")
            montage = Python("MONTAGE\n(scaffolds)")
            arise = Python("ARISE\n(iterative design)")

        with Cluster("External tools"):
            master = Cpp("MASTER\n(CLI subprocess)")

        with Cluster("Java (via JPype)"):
            jpype = Python("JPype\nJVM start")
            java = Java("OSPREY\nK* runs")

        io = Storage("Files\n(PDB/.ccsx/TSV)")

        scope >> Edge(label="PDB/hulls", style="dashed", color="red") >> io >> montage
        montage >> Edge(label="run", style="dashed", color="orange") >> master
        master >> Edge(label="matches", style="dashed", color="red") >> io >> montage

        montage >> Edge(label="prepare", style="dashed", color="red") >> io
        montage >> Edge(label="JPype", style="dashed", color="orange") >> jpype >> java
        java >> Edge(label="results", style="dashed", color="red") >> io >> arise


# -----------------------
# “Pipeline stage” views
# -----------------------


def generate_stage_scope():
    with Diagram(
        "Stage 1: SCOPE (Convex Hull Analysis)",
        filename=f"{OUT_DIR}/stage_scope",
        outformat="png",
        direction="LR",
        graph_attr=_graph_attr(),
        show=False,
    ):
        inputs = Storage("Inputs\nPDB / rotamers")
        geom = Python("Geometry\nalignment")
        hull = Python("Convex hulls\n(intersections)")
        outputs = Storage("Outputs\nhull artifacts")
        inputs >> geom >> hull >> outputs


def generate_stage_montage():
    with Diagram(
        "Stage 2: MONTAGE (Scaffold Generation)",
        filename=f"{OUT_DIR}/stage_montage",
        outformat="png",
        direction="LR",
        graph_attr=_graph_attr(),
        show=False,
    ):
        inputs = Storage("Inputs\nhulls + PDBs")
        master = Cpp("MASTER\nsearch")
        scaffold = Python("Scaffold gen\n+ filtering")
        prep = Python("K* prep\n(.ccsx)")
        outputs = Storage("Outputs\nscaffolds + confspaces")
        inputs >> master >> scaffold >> prep >> outputs


def generate_stage_kstar_execution():
    with Diagram(
        "Stage 3: K* Execution (Current Focus)",
        filename=f"{OUT_DIR}/stage_kstar_execution",
        outformat="png",
        direction="LR",
        graph_attr=_graph_attr(),
        show=False,
    ):
        confspace = Java("ConfSpace load\n(.ccsx)")
        emat = Java("Energy matrix\ncompute")
        pf = Java("Partition functions\n(A*)")
        score = Java("Score sequences\n(K*)")
        out = Storage("Outputs\nTSV + ensembles")
        confspace >> emat >> pf >> score >> out


def generate_stage_arise():
    with Diagram(
        "Stage 4: ARISE (Iterative Design)",
        filename=f"{OUT_DIR}/stage_arise",
        outformat="png",
        direction="LR",
        graph_attr=_graph_attr(),
        show=False,
    ):
        loop = Airflow("Iterate\nDMTA loop")
        propose = Python("Propose\nmutations")
        evaluate = Java("Evaluate\n(K*)")
        analyze = Python("Analyze\nrank + prune")
        loop >> propose >> evaluate >> analyze >> loop


def generate_all():
    os.makedirs(OUT_DIR, exist_ok=True)

    # language layers
    generate_layer_energy_calc()
    generate_layer_java_energy_calc()
    generate_layer_kstar()
    generate_layer_python_workflow()

    # pipeline stages
    generate_stage_scope()
    generate_stage_montage()
    generate_stage_kstar_execution()
    generate_stage_arise()


if __name__ == "__main__":
    generate_all()
    print(f"Done. Wrote PNGs under: {OUT_DIR}/")


