package edu.duke.cs.osprey.energy.forcefield.amber;

import java.io.Serializable;
import java.util.List;

public record EquivalencingAtom(AtomSymbolAndMass IORG, AtomSymbolAndMass IEQV) implements HasAtoms, Serializable {
    @Override
    public List<AtomSymbolAndMass> atoms() {
        return List.of(IORG, IEQV);
    }
}
