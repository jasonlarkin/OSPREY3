package edu.duke.cs.osprey.tools;

import java.io.*;
import java.nio.file.*;

/**
 * Utility to capture serialized Java objects to files for C++ deserializer testing.
 */
public class CaptureSerializedObjects {
    
    /**
     * Capture a serialized object to a binary file.
     * 
     * @param obj Object to serialize and capture
     * @param filename Output filename (will be placed in test-data/serialized/)
     * @throws IOException If serialization or file write fails
     */
    public static void capture(Object obj, String filename) throws IOException {
        // Serialize object
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        ObjectOutputStream oos = new ObjectOutputStream(bos);
        oos.writeObject(obj);
        oos.close();
        
        byte[] data = bos.toByteArray();
        
        // Write to file
        Path outputPath = Paths.get("test-data/serialized", filename);
        Files.createDirectories(outputPath.getParent());
        Files.write(outputPath, data);
        
        System.out.println("Captured: " + filename + " (" + data.length + " bytes)");
    }
    
    /**
     * Capture object with metadata (class name, size) to JSON file.
     */
    public static void captureWithMetadata(Object obj, String filename) throws IOException {
        capture(obj, filename);
        
        // Capture metadata
        int size = getSerializedSize(obj);
        String metadata = String.format(
            "{\n" +
            "  \"class\": \"%s\",\n" +
            "  \"size\": %d,\n" +
            "  \"filename\": \"%s\"\n" +
            "}\n",
            obj.getClass().getName(),
            size,
            filename
        );
        
        Path metadataPath = Paths.get("test-data/metadata", 
            filename.replace(".bin", ".json"));
        Files.createDirectories(metadataPath.getParent());
        Files.write(metadataPath, metadata.getBytes());
        
        System.out.println("Metadata: " + metadataPath);
    }
    
    private static int getSerializedSize(Object obj) throws IOException {
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        ObjectOutputStream oos = new ObjectOutputStream(bos);
        oos.writeObject(obj);
        oos.close();
        return bos.size();
    }
}

