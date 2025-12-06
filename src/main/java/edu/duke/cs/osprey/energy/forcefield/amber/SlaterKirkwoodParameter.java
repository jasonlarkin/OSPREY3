package edu.duke.cs.osprey.energy.forcefield.amber;

import java.io.Serializable;
import java.util.List;

public record SlaterKirkwoodParameter(AtomSymbolAndMass LTYNB, float POL, float XNEFF, float RMIN) implements HasAtoms, Serializable {
    @Override
    public List<AtomSymbolAndMass> atoms() {
        return List.of(LTYNB);
    }
}
