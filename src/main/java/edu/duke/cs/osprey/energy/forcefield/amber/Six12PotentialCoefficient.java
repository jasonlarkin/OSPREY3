package edu.duke.cs.osprey.energy.forcefield.amber;

import java.io.Serializable;
import java.util.List;

public record Six12PotentialCoefficient(AtomSymbolAndMass LTYNB, float A, float C) implements HasAtoms, Serializable {
    @Override
    public List<AtomSymbolAndMass> atoms() {
        return List.of(LTYNB);
    }
}
