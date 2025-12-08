package edu.duke.cs.osprey.tools;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

import org.junit.jupiter.api.Test;

import edu.duke.cs.osprey.TestBase;

/**
 * Capture test objects from BenchmarkDeepCopy for C++ deserializer testing.
 * 
 * This test serializes objects and saves them to test-data/ directory.
 */
public class CaptureTestObjects extends TestBase {
    
    // Copy test object classes from BenchmarkDeepCopy
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
    
    @Test
    public void captureSimpleObject() throws IOException {
        SimpleObject obj = new SimpleObject(42, "test", 3.14);
        CaptureSerializedObjects.captureWithMetadata(obj, "simple-object.bin");
    }
    
    @Test
    public void captureNestedObject() throws IOException {
        NestedObject nested = new NestedObject();
        nested.child = new SimpleObject(1, "child", 2.0);
        nested.children.add(new SimpleObject(2, "child2", 3.0));
        nested.children.add(new SimpleObject(3, "child3", 4.0));
        CaptureSerializedObjects.captureWithMetadata(nested, "nested-object.bin");
    }
    
    @Test
    public void captureCircularNode() throws IOException {
        CircularNode nodeA = new CircularNode(1);
        CircularNode nodeB = new CircularNode(2);
        CircularNode nodeC = new CircularNode(3);
        nodeA.next = nodeB;
        nodeB.next = nodeC;
        nodeC.next = nodeA;  // Circular reference
        
        CaptureSerializedObjects.captureWithMetadata(nodeA, "circular-node.bin");
    }
    
    @Test
    public void captureDeepNested() throws IOException {
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
    }
}

