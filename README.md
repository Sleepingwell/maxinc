maxinc
======

This was a little program I knocked up to convince myself that excel's solver was doing a pretty poor job of optimising the way income should be drawn out of a business over a multi-year horizon... and to refresh my memory on how to write a linear proogram (excel was giving the wrong answer.

The tax regime is that for personal tax in Australia, which changed at the end of the 2011/12 financial year. It would be pretty easy to make the specification of the tax regime more flexible... but I did not need that. It assumes that all income is paid at the end of a financial year. Note that in Australia, dividends come with an imputation credit, so company tax can be ignored (... for a small business and with 'typical' interest rates - more below).

For an interest rate of zero, it is pretty easy (and intuitive) to show that the optimal way to draw money over a multi-year horizon is to try and ensure that the highest marginal tax rate is the same in every year... and one doesn't really need to use this program to do that. For high interest rates, this may not be the case as the value of future income is reduced and it may be more optimal to take money out in earlier periods, even if the tax paid overall increases.

Note that this code does not capture the whole story, as one can pay dividends at the beginning of a financial year. For typical interest rates, this should not change the results significantly, but for high interest rates, it may mean that more income is carried over to lower tax brackets since some of 'future' income is discounted by one period less.

Hopefully the <code>main</code> function is self documenting... but if not, please ask.

Dependencies
------------

The only dependency is [LEMON](http://lemon.cs.elte.hu/trac/lemon/wiki/Documentation) (It puts a nice interface over a couple of solvers - I have only used [GLPK](http://www.gnu.org/software/glpk/)).