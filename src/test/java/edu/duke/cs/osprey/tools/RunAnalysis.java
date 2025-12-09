package edu.duke.cs.osprey.tools;

import org.junit.jupiter.api.Test;
import java.io.*;
import java.nio.file.Files;
import java.nio.file.Paths;

/**
 * Simple test to run the analysis tool and capture output to file.
 */
public class RunAnalysis {
    
    @Test
    public void analyze1CC8() throws IOException {
        // Redirect System.out to a file
        PrintStream originalOut = System.out;
        try {
            File outputFile = new File("test-data/analysis_output.txt");
            outputFile.getParentFile().mkdirs();
            PrintStream fileOut = new PrintStream(new FileOutputStream(outputFile));
            System.setOut(fileOut);
            
            AnalyzeSerializedStructure.main(new String[]{
                "test-data/serialized/1cc8-deeper-molecule-modifier-scorer.bin"
            });
            
            fileOut.close();
            System.out.println("Analysis complete. Output written to: " + outputFile.getAbsolutePath());
        } finally {
            System.setOut(originalOut);
        }
    }
}

