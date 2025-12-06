package edu.duke.cs.osprey.energy.forcefield.amber;

import java.io.Serializable;
import java.util.List;

public record BondLengthParameter(AtomSymbolAndMass IBT, AtomSymbolAndMass JBT, float RK, float REQ) implements HasAtoms, Serializable {

    @Override
    public List<AtomSymbolAndMass> atoms() {
        return List.of(IBT, JBT);
    }

}
