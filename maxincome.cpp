/**
 * Copyright (C) 2013 Simon Knapp
 *
 * This program is free software; you can redistribute it and/or
 * modify it under any terms you wish.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <iostream>
#include <vector>
#include <limits>
#include <cmath>
#include <lemon/lp.h>

// If the marginal tax rates are non-increasing, then the LP will allocate
// income to lower brackets first. If not, then we need to force this to
// occur by adding additional constraints, which is done when the following
// macro is defined.
//#define MARGINAL_TAX_RATES_CAN_DECREASE

// The number of tax brackets. In a more flexible solution, the upper bounds
// and tax rates would become arguments to PersonalTax::addYear (which, being
// means that some refactoring would have to be done).
#define MAXINC_N_TAX_BRACKETS 5

struct Program : lemon::Mip {
    typedef lemon::Mip::Col Variable;
    typedef lemon::Mip::Row Constraint;
    typedef lemon::Mip::Expr Expression;

    Expression& getObjective(void) {
        return objective_;
    }

    void run(void) {
        this->max();
        this->obj(objective_);
        this->solve();
    }

private:
    Expression
        objective_;
};





struct PersonalTax {
    typedef Program::Expression Expression;
    typedef Program::Variable Variable;
    typedef std::vector<double> DoubleArray;
    typedef std::vector<Variable> Variables;

    template<typename YearIter, typename RevenueIter>
    void addToProgram(
        Program& program,
        YearIter bYears, YearIter eYears,
        RevenueIter bRevs, RevenueIter eRevs,
        double interestRate,
        int currentYear
    ) {
        double
            previousRevenue(0.0);

        int
            index(0);

        Expression
            previousWages;

        size_t
            nYears(std::distance(bYears, eYears));

        variables_.resize(nYears*MAXINC_N_TAX_BRACKETS);
        bracketPayments_.resize(nYears*MAXINC_N_TAX_BRACKETS);
#ifdef MARGINAL_TAX_RATES_CAN_DECREASE
        upperBounds_.resize(nYears*MAXINC_N_TAX_BRACKETS);
#endif // MARGINAL_TAX_RATES_CAN_DECREASE

        eYears -= 1;
        for(; bYears!=eYears; ++bYears, ++bRevs, ++index) previousRevenue = addYear(program, index, currentYear, *bYears, interestRate, *bRevs, previousRevenue, previousWages);
        addYear(program, index, currentYear, *bYears, interestRate, *bRevs, previousRevenue, previousWages, true);
    }

    void retrieveOutputs(Program const& program) {
        DoubleArray::iterator
            b(bracketPayments_.begin()),
            e(bracketPayments_.end());

        Variables::const_iterator
            bv(variables_.begin());

        double
            tot(0.0);

        for(int i=0; b!=e; ++b, ++bv) {
            *b = program.sol(*bv);
            tot += *b;
            std::cout << "bracket " << i+1 << " income: " << *b << '\n';
            if(i>0 && !(i%(MAXINC_N_TAX_BRACKETS-1))) {
                std::cout << "total income: " << tot << "\n\n";
                i = 0;
                tot = 0.0;
            } else {
                ++i;
            }
        }
        std::cout << "total income: " << tot << "\n\n";
    }

private:
    double addYear(
        Program& program,
        size_t index,
        int currentYear,
        int year,
        double interestRate,
        double revenue,
        double previousRevenue,
        Expression& previousYearsWages,
        bool isLast=false
    ) {
        size_t
            baseIndex(index*MAXINC_N_TAX_BRACKETS);

        int
            yearIndex(year-currentYear);

        if(year <= 2012) {
            previousYearsWages += addBracket(program, baseIndex, yearIndex, 0.0, 6000.0, 0.0, interestRate);
            previousYearsWages += addBracket(program, baseIndex+1, yearIndex, 6000.0, 37000.0, 0.15, interestRate);
            previousYearsWages += addBracket(program, baseIndex+2, yearIndex, 37000.0, 80000.0, 0.3, interestRate);
            previousYearsWages += addBracket(program, baseIndex+3, yearIndex, 80000.0, 180000.0, 0.37, interestRate);
            previousYearsWages += addBracket(program, baseIndex+4, yearIndex, 180000.0, std::numeric_limits<double>::max(), 0.45, interestRate);
        } else {
            previousYearsWages += addBracket(program, baseIndex, yearIndex, 0.0, 18200.0, 0.0, interestRate);
            previousYearsWages += addBracket(program, baseIndex+1, yearIndex, 18200.0, 37000.0, 0.19, interestRate);
            previousYearsWages += addBracket(program, baseIndex+2, yearIndex, 37000.0, 80000.0, 0.325, interestRate);
            previousYearsWages += addBracket(program, baseIndex+3, yearIndex, 80000.0, 180000.0, 0.37, interestRate);
            previousYearsWages += addBracket(program, baseIndex+4, yearIndex, 180000.0, std::numeric_limits<double>::max(), 0.45, interestRate);
        }
        previousRevenue += revenue;
        if(isLast) program.addRow(previousYearsWages == previousRevenue);
        else program.addRow(previousYearsWages <= previousRevenue);
        return previousRevenue;
    }

    Variable addBracket(
        Program& program,
        size_t index,
        int yearIndex,
        double mn,
        double mx,
        double taxRate,
        double interestRate
    ) {
        Variable
            bracketIncome(variables_[index] = program.addCol());

        program.getObjective() += (1.0 - taxRate) * bracketIncome * (yearIndex > 0 ? pow(1.0+interestRate, -yearIndex) : 1.0);
#ifndef MARGINAL_TAX_RATES_CAN_DECREASE
        program.colBounds(bracketIncome, 0.0, mx-mn);
#else // MARGINAL_TAX_RATES_CAN_DECREASE
        Variable
            fillLastBracket;

        program.colLowerBound(bracketIncome, 0.0);
        upperBounds_[index] = mx-mn;
        switch(index%MAXINC_N_TAX_BRACKETS) {
            case 0:
                program.colUpperBound(bracketIncome, mx-mn);
                break;
            default:
                // We need to make sure that we fill the next lowest income range first.
                // note that since the marginal tax taxRate is strictly increasing, I may
                // not have to be this careful with the constraints, as the program should
                // always fill the lower levels first.
                fillLastBracket = program.addCol();
                program.colType(fillLastBracket, Program::INTEGER);
                program.colBounds(fillLastBracket, 0, 1);
                program.addRow(fillLastBracket <= (variables_[index-1] + 1e-8) / upperBounds_[index-1]); // add just a little bit to deal with rounding
                program.addRow(bracketIncome <= fillLastBracket * (mx-mn));
                break;
        }
#endif // MARGINAL_TAX_RATES_CAN_DECREASE
        return bracketIncome;
    }

private:
    Variables
        variables_;

    DoubleArray
#ifdef MARGINAL_TAX_RATES_CAN_DECREASE
        upperBounds_,
#endif // MARGINAL_TAX_RATES_CAN_DECREASE
        bracketPayments_;
};





int main(int argc, char* argv[]) {
    double
        revenues[] = {30000.0, 100000.0, 40000.0};

    int
        years[] = {2012, 2013, 2014};

    size_t
        nYears(sizeof(years) / sizeof(int));

    int
        currentYear(2013);

    double
        interestRate(0.055);

    Program
        program;

    PersonalTax
        pt;

    pt.addToProgram(program, years, years+nYears, revenues, revenues+nYears, interestRate, currentYear);
    program.run();
    switch(program.type()) {
        case Program::UNDEFINED:
            std::cout << "Feasible solution hasn't been found (but may exist).\n";
            return 1;
        case Program::INFEASIBLE:
            std::cout << "The problem has no feasible solution.\n";
            return 1;
        case Program::FEASIBLE:
            std::cout << "Feasible solution found.\n";
            return 0;
        case Program::OPTIMAL:
            std::cout << "Optimal solution exists and found.\n";
            pt.retrieveOutputs(program);
            return 0;
        case Program::UNBOUNDED:
            std::cout << "The cost function is unbounded.\n\tThe Mip or at least the relaxed problem is unbounded.";
            return 1;
    }
}
