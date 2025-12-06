package edu.duke.cs.osprey.energy.forcefield.amber;

import java.io.Serializable;

public record AtomSymbolAndMass(String KNDSYM, float AMASS/*, float ATPOL*/) implements Serializable { } // Polarity is not supplied, though it's in standard
