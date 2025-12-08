package edu.duke.cs.osprey.tools;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

/**
 * Standalone utility to generate test data for C++ deserializer testing.
 * 
 * This can be run directly without the full test framework:
 *   java -cp <classpath> edu.duke.cs.osprey.tools.GenerateTestData
 * 
 * Or via Gradle:
 *   ./gradlew run -PmainClass=edu.duke.cs.osprey.tools.GenerateTestData
 */
public class GenerateTestData {
    
    // Test object classes (same as CaptureTestObjects)
    static class SimpleObject implements java.io.Serializable {
        private static final long serialVersionUID = 1L;
        int value;
        String name;
        double data;
        
        SimpleObject(int v, String n, double d) {
            value = v;
            name = n;
            data = d;
        }
    }
    
    static class NestedObject implements java.io.Serializable {
        private static final long serialVersionUID = 1L;
        SimpleObject child;
        List<SimpleObject> children;
        NestedObject nested;
        
        NestedObject() {
            children = new ArrayList<>();
        }
    }
    
    static class CircularNode implements java.io.Serializable {
        private static final long serialVersionUID = 1L;
        int value;
        CircularNode next;
        
        CircularNode(int v) {
            value = v;
        }
    }
    
    public static void main(String[] args) {
        System.out.println("Generating test data for C++ deserializer...");
        
        try {
            // Setup directories
            java.nio.file.Files.createDirectories(
                java.nio.file.Paths.get("test-data/serialized"));
            java.nio.file.Files.createDirectories(
                java.nio.file.Paths.get("test-data/metadata"));
            
            // Generate SimpleObject
            System.out.println("Generating simple-object.bin...");
            SimpleObject simple = new SimpleObject(42, "test", 3.14);
            CaptureSerializedObjects.captureWithMetadata(simple, "simple-object.bin");
            
            // Generate NestedObject
            System.out.println("Generating nested-object.bin...");
            NestedObject nested = new NestedObject();
            nested.child = new SimpleObject(1, "child", 2.0);
            nested.children.add(new SimpleObject(2, "child2", 3.0));
            nested.children.add(new SimpleObject(3, "child3", 4.0));
            CaptureSerializedObjects.captureWithMetadata(nested, "nested-object.bin");
            
            // Generate CircularNode
            System.out.println("Generating circular-node.bin...");
            CircularNode nodeA = new CircularNode(1);
            CircularNode nodeB = new CircularNode(2);
            CircularNode nodeC = new CircularNode(3);
            nodeA.next = nodeB;
            nodeB.next = nodeC;
            nodeC.next = nodeA;  // Circular reference
            CaptureSerializedObjects.captureWithMetadata(nodeA, "circular-node.bin");
            
            // Generate DeepNested
            System.out.println("Generating deep-nested.bin...");
            NestedObject root = new NestedObject();
            NestedObject current = root;
            
            // Create 10 levels of nesting
            for (int i = 0; i < 10; i++) {
                current.child = new SimpleObject(i, "level" + i, i * 0.1);
                if (i < 9) {
                    current.nested = new NestedObject();
                    current = current.nested;
                }
            }
            CaptureSerializedObjects.captureWithMetadata(root, "deep-nested.bin");
            
            System.out.println("\nTest data generation complete!");
            System.out.println("Files saved to: test-data/serialized/ and test-data/metadata/");
            
        } catch (IOException e) {
            System.err.println("Error generating test data: " + e.getMessage());
            e.printStackTrace();
            System.exit(1);
        }
    }
}

