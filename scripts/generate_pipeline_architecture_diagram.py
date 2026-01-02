#!/usr/bin/env python3
"""
Generate architecture diagrams for OSPREY pipeline parallelism analysis.
Uses the diagrams library to create visual representations of the pipeline.
"""

import os
from diagrams import Diagram, Cluster, Edge
from diagrams.onprem.client import Client
from diagrams.onprem.compute import Server
from diagrams.onprem.workflow import Airflow
from diagrams.programming.language import Python, Java, Cpp
from diagrams.onprem.container import Docker
from diagrams.generic.storage import Storage  # Use Storage instead of Database
from diagrams.aws.compute import EC2
from diagrams.aws.storage import S3

def generate_current_architecture_diagram():
    """Generate diagram showing current OSPREY architecture with bottlenecks."""
    
    graph_attr = {
        "bgcolor": "white",
        "dpi": "300",
        "pad": "0.5"
    }
    
    with Diagram("OSPREY Current Pipeline Architecture (Multi-Language)", 
                 filename="docs/performance/images/pipeline_current",
                 outformat="png",
                 direction="LR",
                 graph_attr=graph_attr,
                 show=False):
        
        # Python Orchestration Layer
        with Cluster("Python Orchestration Layer (CCKStar)"):
            python_scope = Python("SCOPE\nConvex Hulls")
            python_montage = Python("MONTAGE\nScaffold Gen")
            python_arise = Python("ARISE\nIterative Design")
            
        # External Tools
        with Cluster("External Tools (Subprocess)"):
            master_cpp = Cpp("MASTER\nC++ CLI")
            
        # Java Core
        with Cluster("Java Core (OSPREY)"):
            java_confspace = Java("ConfSpace\nCompilation")
            java_kstar = Java("K* Algorithm\nA* Search")
            java_parallel = Java("Parallelism\nTaskExecutor")
            
        # C++ Native Layer
        with Cluster("C++ Native Layer (ConfEcalc)"):
            cpp_energy = Cpp("Energy Calc\nAmber/EEF1")
            cpp_minimize = Cpp("CCD Min\nOpenMP")
            
        # File I/O bottlenecks
        file_io = Storage("File I/O\nBottleneck")
        
        # Flow connections
        python_scope >> Edge(label="PDB files", style="dashed", color="red") >> file_io
        file_io >> Edge(label="hull files", style="dashed", color="red") >> python_montage
        
        python_montage >> Edge(label="subprocess", style="dashed", color="orange") >> master_cpp
        master_cpp >> Edge(label="match PDBs", style="dashed", color="red") >> file_io
        file_io >> Edge(label="scaffolds", style="dashed", color="red") >> python_montage
        
        python_montage >> Edge(label="JPype", style="dashed", color="orange") >> java_confspace
        java_confspace >> Edge(label=".ccsx files", style="dashed", color="red") >> file_io
        file_io >> Edge(label="load", style="dashed", color="red") >> java_kstar
        
        java_kstar >> Edge(label="JNA", style="dashed", color="orange") >> cpp_energy
        cpp_energy >> cpp_minimize
        cpp_minimize >> Edge(label="energy values", style="dashed") >> java_kstar
        
        java_kstar >> Edge(label="parallel tasks", style="solid", color="green") >> java_parallel
        java_parallel >> Edge(label="threads", style="solid", color="green") >> cpp_energy
        
        java_kstar >> Edge(label="results TSV", style="dashed", color="red") >> file_io
        file_io >> Edge(label="sequences", style="dashed", color="red") >> python_arise

def generate_unified_architecture_diagram():
    """Generate diagram showing a unified optimized architecture (portable, non-proprietary)."""
    
    graph_attr = {
        "bgcolor": "white",
        "dpi": "300",
        "pad": "0.5"
    }
    
    with Diagram("Unified Pipeline Architecture (C++ Unified)",
                 filename="docs/performance/images/pipeline_unified",
                 outformat="png",
                 direction="LR",
                 graph_attr=graph_attr,
                 show=False):
        
        # Unified C++ Pipeline
        with Cluster("Unified C++ Pipeline"):
            cpp_scope = Cpp("Geometric\nAnalysis")
            cpp_search = Cpp("Structural\nSearch")
            cpp_confspace = Cpp("ConfSpace\nCompilation")
            cpp_energy = Cpp("Energy\nCalculation")
            cpp_kstar = Cpp("K* Algorithm\nA* Search")
            
            # GPU layer
            with Cluster("GPU Acceleration"):
                cuda_energy = Server("CUDA\nEnergy Calc")
                
        # Unified Memory
        unified_memory = Storage("Unified Memory\nZero-Copy")
        
        # Cloud/Cluster
        with Cluster("Cloud/Hardware"):
            cloud = EC2("Scalable\nCompute")
            gpu_hw = Server("GPU\nHardware")
        
        # Flow connections (all in-memory, no serialization)
        cpp_scope >> Edge(label="in-memory", style="solid", color="green") >> unified_memory
        unified_memory >> Edge(label="zero-copy", style="solid", color="green") >> cpp_search
        cpp_search >> Edge(label="in-memory", style="solid", color="green") >> unified_memory
        unified_memory >> Edge(label="zero-copy", style="solid", color="green") >> cpp_confspace
        cpp_confspace >> Edge(label="in-memory", style="solid", color="green") >> unified_memory
        unified_memory >> Edge(label="zero-copy", style="solid", color="green") >> cpp_energy
        
        cpp_energy >> Edge(label="CUDA", style="solid", color="blue") >> cuda_energy
        cuda_energy >> Edge(label="GPU mem", style="solid", color="blue") >> gpu_hw
        
        unified_memory >> Edge(label="zero-copy", style="solid", color="green") >> cpp_kstar
        cpp_kstar >> Edge(label="parallel", style="solid", color="green") >> cloud

def generate_parallelism_comparison_diagram():
    """Generate diagram comparing sequential vs parallel execution patterns."""
    
    graph_attr = {
        "bgcolor": "white",
        "dpi": "300",
        "pad": "0.5"
    }
    
    with Diagram("Parallelism Comparison: Current vs Optimized", 
                 filename="docs/performance/images/parallelism_comparison",
                 outformat="png",
                 direction="TB",
                 graph_attr=graph_attr,
                 show=False):
        
        # Current Sequential
        with Cluster("Current: Sequential Execution"):
            match1_seq = Server("Match 1\nK* Run")
            match2_seq = Server("Match 2\nK* Run")
            match3_seq = Server("Match 3\nK* Run")
            match10_seq = Server("Match 10\nK* Run")
            
            match1_seq >> Edge(label="sequential", style="dashed", color="red") >> match2_seq
            match2_seq >> Edge(label="sequential", style="dashed", color="red") >> match3_seq
            match3_seq >> Edge(label="...", style="dashed", color="red") >> match10_seq
            
            jvm1 = Java("JVM 1\n1GB heap")
            jvm2 = Java("JVM 2\n1GB heap")
            jvm10 = Java("JVM 10\n1GB heap")
            
            match1_seq >> jvm1
            match2_seq >> jvm2
            match10_seq >> jvm10
        
        # Optimized Parallel
        with Cluster("Optimized: Parallel Execution"):
            orchestrator = Python("Orchestrator\nLong-running")
            unified_jvm = Java("Shared JVM\nUnified Memory")
            
            with Cluster("Parallel Matches"):
                match1_par = Server("Match 1")
                match2_par = Server("Match 2")
                match3_par = Server("Match 3")
                match10_par = Server("Match 10")
                
            orchestrator >> Edge(label="parallel", style="solid", color="green") >> match1_par
            orchestrator >> Edge(label="parallel", style="solid", color="green") >> match2_par
            orchestrator >> Edge(label="parallel", style="solid", color="green") >> match3_par
            orchestrator >> Edge(label="parallel", style="solid", color="green") >> match10_par
            
            match1_par >> unified_jvm
            match2_par >> unified_jvm
            match3_par >> unified_jvm
            match10_par >> unified_jvm

if __name__ == "__main__":
    # Create output directory
    os.makedirs("docs/performance/images", exist_ok=True)
    
    print("Generating pipeline architecture diagrams...")
    print("1. Current OSPREY architecture...")
    generate_current_architecture_diagram()
    
    print("2. Unified architecture...")
    generate_unified_architecture_diagram()
    
    print("3. Parallelism comparison...")
    generate_parallelism_comparison_diagram()
    
    print("Done! Diagrams saved to docs/performance/images/")

