#pragma once

// Main include file for generalized molecular energy and search interfaces

#include "ISystem.h"
#include "IState.h"
#include "IEnergyTerm.h"
#include "IEnergyEvaluator.h"
#include "IConformation.h"
#include "IMove.h"
#include "INeighborhood.h"
#include "ISearchProblem.h"
#include "ISearchAlgorithm.h"

/**
 * Generalized molecular energy and search interfaces.
 * 
 * Provides abstract interfaces for:
 * - Molecular system representation (ISystem, IState)
 * - Energy evaluation (IEnergyTerm, IEnergyEvaluator)
 * - Conformation search (IConformation, IMove, INeighborhood, ISearchProblem, ISearchAlgorithm)
 */
