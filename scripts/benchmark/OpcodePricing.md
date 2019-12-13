# Opcode Pricing

We have designed a [pricing mechanism](https://www.overleaf.com/read/qdgrnfdspjgd) to maximize transaction throughput in the fetch.ai ledger by incentivizing transactions to use a small number of resource lanes (or shards) and occupy the least congested lanes. Such a policy promotes efficient operation of an ideal ledger, but it is based on the assumption that transaction fees depend on lane usage alone. Ultimately, the price of transactions should also relate to the actual cost of maintaining the ledger, which in the case of digital tokens, is proportional to the resources consumed by the servers responsible for validating transactions. At a basic level, this can be expressed in terms of the expected number of CPU cycles and memory space needed to validate a transaction. Since transactions include smart contracts and are expressed in a programming language (Etch), they consist of a sequence of machine instructions. Estimates of the CPU time required by a transaction can therefore be decomposed into the times of the constituent instructions or opcodes.

In the fetch.ai distributed ledger, transactions are typically submitted via [Etch-code](https://docs.fetch.ai/etch-language/). Therefore, we estimate the CPU requirements of each opcode in the fetch.ai virtual machine by isolating it as much as possible in a compiled piece of Etch code. For example, if we want to compute the CPU requirements for adding two 32-bit integers, we can compile and time the following Etch code:
```c
function PrimitiveAdd()
var x : Int32 = 1i32;
x + x;
endfunction
```
If we look at the opcodes that result from compiling this code, we get:

	7 PushConstant
	2 VariableDeclareAssign
	8 PushVariable
	8 PushVariable
	48 PrimitiveAdd
	14 Discard
	21 Return

Since we are interested in timing the add operation, we also time the following code for use as as a baseline:
```c
function PushVariable()
var x : Int32 = 1i32;
x;
endfunction
```
This code runs the following opcodes:

	7 PushConstant
	2 VariableDeclareAssign
	8 PushVariable
	14 Discard
	21 Return

By subtracting the time of **PushVariable** from **PrimitiveAdd**, we obtain an estimate of the opcodes 8 (**PushVariable**) and 48 (**PrimitiveAdd**). Note that the primitive add operation requires pushing two variables to the stack. Since the arithmetic operations will almost always be executed after a push operation, we include both together in the benchmark timings. Similarly for the discard operation (14) after a push. However, we will show one way to estimate the isolated opcode times in the next section. Using the [Google benchmark library](https://github.com/google/benchmark) to run 100 repetitions of each compiled function, we obtain the following output:

	Run on (8 X 4000 MHz CPU s)
	CPU Caches:
	L1 Data 32K (x4)
	L1 Instruction 32K (x4)
	L2 Unified 256K (x4)
	L3 Unified 8192K (x1)
	------------------------------------------------------------------------------------
	Benchmark Time CPU Iterations
	------------------------------------------------------------------------------------
	OpcodeBenchmark/PushVariable_mean 53 ns 53 ns 13084724
	OpcodeBenchmark/PushVariable_median 53 ns 53 ns 13084724
	OpcodeBenchmark/PushVariable_stddev 0 ns 0 ns 13084724
	OpcodeBenchmark/PrimitiveAdd_mean 64 ns 64 ns 11084146
	OpcodeBenchmark/PrimitiveAdd_median 64 ns 64 ns 11084146
	OpcodeBenchmark/PrimitiveAdd_stddev 1 ns 1 ns 11084146

Since the measure of interest is the mean CPU time, we compute the standard error in the mean: &sigma;/*n*, where &sigma; denotes the standard deviation and *n* the number of repetitions. By this methodology, we can generate tables like the following, showing the mean CPU times for opcodes that do not depend on a variable or primitive.

### <a name="table1">Table 1</a>: CPU times for basic type-independent benchmarks
| Benchmark (100 reps)   |   Mean (ns) |   Net mean (ns) |   Std error (ns) | Opcodes                           | Baseline     | Net opcodes            |
|------------------------|-------------|-----------------|------------------|-----------------------------------|--------------|------------------------|
| Return                 |       34.09 |            0.00 |             0.21 | [21]                              | Return       | []                     |
| PushFalse              |       53.95 |           19.86 |             0.15 | [4, 14, 21]                       | Return       | [4, 14]                |
| PushTrue               |       42.86 |            8.77 |             0.17 | [5, 14, 21]                       | Return       | [5, 14]                |
| JumpIfFalse            |       52.58 |           18.49 |             0.15 | [4, 19, 21]                       | Return       | [4, 19]                |
| Jump                   |       52.55 |           -0.02 |             0.05 | [4, 19, 18, 21]                   | JumpIfFalse  | [18]                   |
| Not                    |       47.00 |            4.14 |             0.12 | [5, 33, 14, 21]                   | PushTrue     | [33]                   |
| And                    |       51.55 |            8.69 |             0.15 | [5, 31, 5, 14, 21]                | PushTrue     | [31, 5]                |
| Or                     |       59.75 |           16.89 |             0.09 | [4, 32, 5, 14, 21]                | PushTrue     | [4, 32]                |
| ForLoop                |       78.66 |           44.57 |             0.17 | [7, 7, 23, 24, 18, 25, 21]        | Return       | [7, 7, 23, 24, 18, 25] |
| Break                  |       67.18 |          -11.48 |             0.10 | [7, 7, 23, 24, 16, 18, 25, 21]    | ForLoop      | [16]                   |
| Continue               |       78.14 |           -0.52 |             0.35 | [7, 7, 23, 24, 17, 18, 25, 21]    | ForLoop      | [17]                   |
| DestructBase           |       89.05 |           10.39 |             0.12 | [1, 7, 7, 23, 24, 18, 25, 21]     | ForLoop      | [1]                    |
| Destruct               |       90.69 |            1.64 |             0.17 | [7, 7, 23, 24, 1, 15, 18, 25, 21] | DestructBase | [15]                   |
| Function               |       50.71 |           16.62 |             0.15 | [26, 21]                          | Return       | [26]                   |
| VariableDeclareStr     |       40.48 |            6.39 |             0.15 | [1, 21]                           | Return       | [1]                    |

## Least-squares estimation of isolated opcode times

In the previous section, we found that some of the opcodes always appear in pairs. For example, arithmetic operations are almost always preceded by a PushVariable operation and isolated push operations are almost always followed by a Discard operation. As we will see later on, this also arises in more complex operations. Since it would be ideal to measure the CPU times for each opcode in isolation, we show one way to estimate these times here.

Let *A* be a matrix where each row corresponds to a benchmark and each element *A<sub>ij</sub>=k* if benchmark *i* uses opcode a total of *k* times. Let ***b*** be a vector where each element *b<sub>i</sub>* is the mean measured CPU time corresponding to benchmark *i*. Then the solution to ***x***<sup>\*</sup> = arg min(||*A****x*** - ***b***||<sup>2</sup>) provides an estimate of the individual opcode times, where each *x<sub>i</sub>* represents the CPU time required by opcode *i*.

For the benchmarks listed in [Table 1](#table1), we seek values in the vector ***x*** that make the following expression as close as possible to an equality.

![](http://www.sciweavers.org/tex2img.php?eq=%5Csetcounter%7BMaxMatrixCols%7D%7B20%7D%5Cbegin%7Bbmatrix%7D%0A%20%200%20%26%200%20%26%200%20%26%200%20%26%200%20%26%200%20%26%200%20%26%200%20%26%201%20%26%200%20%26%200%20%26%200%20%26%200%20%26%200%20%26%200%20%26%200%5C%5C%0A%20%200%20%26%201%20%26%200%20%26%200%20%26%201%20%26%200%20%26%200%20%26%200%20%26%201%20%26%200%20%26%200%20%26%200%20%26%200%20%26%200%20%26%200%20%26%200%5C%5C%0A%20%200%20%26%200%20%26%201%20%26%200%20%26%201%20%26%200%20%26%200%20%26%200%20%26%201%20%26%200%20%26%200%20%26%200%20%26%200%20%26%200%20%26%200%20%26%200%5C%5C%0A%20%200%20%26%201%20%26%200%20%26%200%20%26%200%20%26%200%20%26%200%20%26%201%20%26%201%20%26%200%20%26%200%20%26%200%20%26%200%20%26%200%20%26%200%20%26%200%5C%5C%0A%20%200%20%26%201%20%26%200%20%26%200%20%26%200%20%26%200%20%26%201%20%26%201%20%26%201%20%26%200%20%26%200%20%26%200%20%26%200%20%26%200%20%26%200%20%26%200%5C%5C%0A%20%200%20%26%200%20%26%201%20%26%200%20%26%201%20%26%200%20%26%200%20%26%200%20%26%201%20%26%200%20%26%200%20%26%200%20%26%200%20%26%200%20%26%200%20%26%201%5C%5C%0A%20%200%20%26%200%20%26%202%20%26%200%20%26%201%20%26%200%20%26%200%20%26%200%20%26%201%20%26%200%20%26%200%20%26%200%20%26%200%20%26%201%20%26%200%20%26%200%5C%5C%0A%20%200%20%26%201%20%26%201%20%26%200%20%26%201%20%26%200%20%26%200%20%26%200%20%26%201%20%26%200%20%26%200%20%26%200%20%26%200%20%26%200%20%26%201%20%26%200%5C%5C%0A%20%200%20%26%200%20%26%200%20%26%202%20%26%200%20%26%200%20%26%201%20%26%200%20%26%201%20%26%201%20%26%201%20%26%201%20%26%200%20%26%200%20%26%200%20%26%200%5C%5C%0A%20%201%20%26%200%20%26%200%20%26%202%20%26%200%20%26%200%20%26%201%20%26%200%20%26%201%20%26%201%20%26%201%20%26%201%20%26%200%20%26%200%20%26%200%20%26%200%5C%5C%0A%20%201%20%26%200%20%26%200%20%26%202%20%26%200%20%26%201%20%26%201%20%26%200%20%26%201%20%26%201%20%26%201%20%26%201%20%26%200%20%26%200%20%26%200%20%26%200%5C%5C%0A%20%200%20%26%200%20%26%200%20%26%200%20%26%200%20%26%200%20%26%200%20%26%200%20%26%201%20%26%200%20%26%200%20%26%200%20%26%201%20%26%200%20%26%200%20%26%200%5C%5C%0A%20%201%20%26%200%20%26%200%20%26%200%20%26%200%20%26%200%20%26%200%20%26%200%20%26%201%20%26%200%20%26%200%20%26%200%20%26%200%20%26%200%20%26%200%20%26%200%5C%5C%0A%5Cend%7Bbmatrix%7D%5Csetcounter%7BMaxMatrixCols%7D%7B20%7D%5Cbegin%7Bbmatrix%7D%0A%20%20x_%7B0%7D%5C%5C%0A%20%20x_%7B1%7D%5C%5C%0A%20%20x_%7B2%7D%5C%5C%0A%20%20x_%7B3%7D%5C%5C%0A%20%20x_%7B4%7D%5C%5C%0A%20%20x_%7B5%7D%5C%5C%0A%20%20x_%7B6%7D%5C%5C%0A%20%20x_%7B7%7D%5C%5C%0A%20%20x_%7B8%7D%5C%5C%0A%20%20x_%7B9%7D%5C%5C%0A%20%20x_%7B10%7D%5C%5C%0A%20%20x_%7B11%7D%5C%5C%0A%20%20x_%7B12%7D%5C%5C%0A%20%20x_%7B13%7D%5C%5C%0A%20%20x_%7B14%7D%5C%5C%0A%20%20x_%7B15%7D%5C%5C%0A%5Cend%7Bbmatrix%7D%20%5Capprox%20%5Csetcounter%7BMaxMatrixCols%7D%7B20%7D%5Cbegin%7Bbmatrix%7D%0A%20%2034.09%5C%5C%0A%20%2053.95%5C%5C%0A%20%2042.86%5C%5C%0A%20%2052.58%5C%5C%0A%20%2052.55%5C%5C%0A%20%2047.%5C%5C%0A%20%2051.55%5C%5C%0A%20%2059.75%5C%5C%0A%20%2078.66%5C%5C%0A%20%2089.05%5C%5C%0A%20%2090.69%5C%5C%0A%20%2050.71%5C%5C%0A%20%2040.48%5C%5C%0A%5Cend%7Bbmatrix%7D&bc=White&fc=Black&im=png&fs=12&ff=modern&edit=)

In addition to minimizing ||*A****x*** - ***b***||<sup>2</sup>, we also need to constrain the individual opcode times to be positive (*x<sub>i</sub> > 0* for each *i*). Finally, since CPU times vary more for some benchmarks than others, we normalize the expression by the standard deviation and rewrite the expression as follows:

![](http://www.sciweavers.org/tex2img.php?eq=%5Cmathbf%7Bx%7D%5E%2A%20%3A%3D%20%5Carg%5Cmin_%7B%5Cmathbf%7Bx%7D%5Cgeq%200%7D%7C%7C%5Chat%7BA%7D%5Cmathbf%7Bx%7D-%5Chat%7B%5Cmathbf%7Bb%7D%7D%7C%7C%5E2%2C%20%5Cqquad%20%5Ctext%7Bwhere%20%7D%20%5Chat%7BA%7D%20%3A%3D%20%5B%5Cfrac%7Ba_%7Bij%7D%7D%7B%5Csigma_i%7D%5D_%7Bm%5Ctimes%20n%7D%2C%20%5C%2C%20%20%20%5Chat%7B%5Cmathbf%7Bb%7D%7D%3A%3D%5B%5Cfrac%7Bb_1%7D%7B%5Csigma_1%7D%2C%5Cdots%2C%5Cfrac%7Bb_m%7D%7B%5Csigma_m%7D%5D&bc=White&fc=Black&im=png&fs=12&ff=modern&edit=)

The solution of the above example is:

![](http://www.sciweavers.org/tex2img.php?eq=%5Cmathbf%7Bx%7D%5E%2A%20%3D%20%5Csetcounter%7BMaxMatrixCols%7D%7B20%7D%5Cbegin%7Bbmatrix%7D%0A%20%20x_%7B0%7D%5C%5C%0A%20%20x_%7B1%7D%5C%5C%0A%20%20x_%7B2%7D%5C%5C%0A%20%20x_%7B3%7D%5C%5C%0A%20%20x_%7B4%7D%5C%5C%0A%20%20x_%7B5%7D%5C%5C%0A%20%20x_%7B6%7D%5C%5C%0A%20%20x_%7B7%7D%5C%5C%0A%20%20x_%7B8%7D%5C%5C%0A%20%20x_%7B9%7D%5C%5C%0A%20%20x_%7B10%7D%5C%5C%0A%20%20x_%7B11%7D%5C%5C%0A%20%20x_%7B12%7D%5C%5C%0A%20%20x_%7B13%7D%5C%5C%0A%20%20x_%7B14%7D%5C%5C%0A%20%20x_%7B15%7D%5C%5C%0A%5Cend%7Bbmatrix%7D%3D%5Csetcounter%7BMaxMatrixCols%7D%7B20%7D%5Cbegin%7Bbmatrix%7D%0A%20%208.9%5C%5C%0A%20%2014.72%5C%5C%0A%20%203.62%5C%5C%0A%20%2013.71%5C%5C%0A%20%207.48%5C%5C%0A%20%202.27%5C%5C%0A%20%200.%5C%5C%0A%20%206.09%5C%5C%0A%20%2031.76%5C%5C%0A%20%206.78%5C%5C%0A%20%206.78%5C%5C%0A%20%206.78%5C%5C%0A%20%2018.95%5C%5C%0A%20%205.08%5C%5C%0A%20%202.17%5C%5C%0A%20%204.14%5C%5C%0A%5Cend%7Bbmatrix%7D&bc=White&fc=Black&im=png&fs=12&ff=modern&edit=)

where the values of **x** are given in nanoseconds and the corresponding opcodes are defined as {1: VariableDeclare, 4: PushFalse, 5: PushTrue, 7: PushConstant, 14: Discard, 15: Destruct, 18: Jump, 19: JumpIfFalse,  21: Return, 23: ForRangeInit, 24: ForRangeIterate, 25: ForRangeTerminate, 26: InvokeUserDefinedFreeFunction, 31: JumpIfFalseOrPop, 32: JumpIfTrueOrPop, 33: Not}.

We can then use these estimates to decouple the opcode times listed in [Table 1](#table1), which can then be subtracted wherever they appear in other benchmarks. Discrepancies between the Mean column and total estimated time reflect the fact that the least-squares fit is necessarily an approximation.

### <a name="table2">Table 2</a>: Basic benchmarks decomposed into estimated individual opcode times
| Benchmark (100 reps)   |   Mean (ns) |   Std. error (ns) | Opcodes: estimated times (ns)                                                  |
|------------------------|-------------|-------------------|--------------------------------------------------------------------------------|
| Return                 |       34.09 |              0.21 | {21: 31.76}                                                                    |
| PushFalse              |       53.95 |              0.15 | {4: 14.72, 14: 7.48, 21: 31.76}                                                |
| PushTrue               |       42.86 |              0.17 | {5: 3.62, 14: 7.48, 21: 31.76}                                                 |
| JumpIfFalse            |       52.58 |              0.15 | {4: 14.72, 19: 6.09, 21: 31.76}                                                |
| Jump                   |       52.55 |              0.05 | {4: 14.72, 19: 6.09, 18: 0.0, 21: 31.76}                                       |
| Not                    |       47.00 |              0.12 | {5: 3.62, 33: 4.14, 14: 7.48, 21: 31.76}                                       |
| And                    |       51.55 |              0.15 | {5: 3.62, 31: 5.08, 14: 7.48, 21: 31.76}                                       |
| Or                     |       59.75 |              0.09 | {4: 14.72, 32: 2.17, 5: 3.62, 14: 7.48, 21: 31.76}                             |
| ForLoop                |       78.66 |              0.17 | {7: 13.71, 23: 6.78, 24: 6.78, 18: 0.0, 25: 6.78, 21: 31.76}                   |
| DestructBase           |       89.05 |              0.12 | {1: 8.9, 7: 13.71, 23: 6.78, 24: 6.78, 18: 0.0, 25: 6.78, 21: 31.76}           |
| Destruct               |       90.69 |              0.17 | {7: 13.71, 23: 6.78, 24: 6.78, 1: 8.9, 15: 2.27, 18: 0.0, 25: 6.78, 21: 31.76} |
| Function               |       50.71 |              0.15 | {26: 18.95, 21: 31.76}                                                         |
| VariableDeclareStr     |       40.48 |              0.15 | {1: 8.9, 21: 31.76}                                                            |

See [Appendix](#app) for the list of all parameter-independent opcodes.

## Primitive opcodes

We tested the primitive opcodes for each of the different primitives to determine if there is any significant difference in CPU time. [Table 3](#table3) shows that the different integer types result only in very small variations in CPU time.

### <a name="table3">Table 3</a>: Mean CPU times for integer primitive opcodes (+/- std error)
| Benchmark (100 reps)        | Int16 (ns)   | Int32 (ns)   | Int64 (ns)   | Int8 (ns)    | UInt16 (ns)   | UInt32 (ns)   | UInt64 (ns)   | UInt8 (ns)   |
|-----------------------------|--------------|--------------|--------------|--------------|---------------|---------------|---------------|--------------|
| PrimAdd                     | 11.46 ± 0.15 | 11.05 ± 0.13 | 12.06 ± 0.10 | 10.24 ± 0.11 | 11.96 ± 0.07  | 11.41 ± 0.19  | 11.88 ± 0.05  | 11.88 ± 0.06 |
| PrimDivide                  | 14.75 ± 0.12 | 14.59 ± 0.25 | 18.23 ± 0.10 | 15.59 ± 0.22 | 14.68 ± 0.07  | 14.19 ± 0.20  | 17.88 ± 0.10  | 14.16 ± 0.08 |
| PrimEqual                   | 13.80 ± 0.10 | 13.59 ± 0.07 | 13.67 ± 0.07 | 15.58 ± 0.28 | 14.02 ± 0.07  | 13.59 ± 0.19  | 14.21 ± 0.06  | 15.33 ± 0.24 |
| PrimGreaterThan             | 14.13 ± 0.09 | 14.02 ± 0.08 | 13.37 ± 0.12 | 15.09 ± 0.22 | 14.13 ± 0.12  | 14.11 ± 0.19  | 14.23 ± 0.06  | 14.10 ± 0.06 |
| PrimGreaterThanOrEqual      | 13.80 ± 0.09 | 14.05 ± 0.07 | 12.90 ± 0.12 | 14.63 ± 0.15 | 13.94 ± 0.09  | 13.93 ± 0.19  | 13.88 ± 0.06  | 14.15 ± 0.07 |
| PrimLessThan                | 14.38 ± 0.14 | 13.74 ± 0.06 | 13.23 ± 0.09 | 15.20 ± 0.21 | 14.01 ± 0.14  | 14.55 ± 0.20  | 13.93 ± 0.10  | 14.27 ± 0.07 |
| PrimLessThanOrEqual         | 14.14 ± 0.09 | 13.44 ± 0.10 | 13.38 ± 0.12 | 14.99 ± 0.17 | 14.66 ± 0.07  | 14.03 ± 0.19  | 14.23 ± 0.07  | 14.44 ± 0.09 |
| PrimMultiply                | 14.83 ± 0.09 | 14.18 ± 0.06 | 14.25 ± 0.06 | 14.34 ± 0.23 | 14.82 ± 0.08  | 13.87 ± 0.20  | 14.32 ± 0.06  | 13.95 ± 0.09 |
| PrimNegate                  | 4.70 ± 0.08  | 4.50 ± 0.06  | 4.78 ± 0.10  | 5.71 ± 0.20  | 5.05 ± 0.07   | 4.65 ± 0.19   | 5.48 ± 0.08   | 5.21 ± 0.07  |
| PrimNotEqual                | 14.13 ± 0.12 | 13.80 ± 0.08 | 13.15 ± 0.12 | 16.03 ± 0.27 | 14.21 ± 0.12  | 13.82 ± 0.23  | 14.02 ± 0.06  | 14.39 ± 0.07 |
| PrimPopToVariable           | 9.63 ± 0.06  | 9.70 ± 0.07  | 9.19 ± 0.13  | 9.03 ± 0.04  | 9.49 ± 0.04   | 9.35 ± 0.04   | 9.87 ± 0.20   | 9.28 ± 0.07  |
| PrimPushConst               | 9.07 ± 0.15  | 8.86 ± 0.15  | 8.79 ± 0.15  | 8.33 ± 0.15  | 9.11 ± 0.21   | 9.02 ± 0.15   | 8.56 ± 0.15   | 8.61 ± 0.15  |
| PrimPushVariable            | 10.50 ± 0.08 | 10.87 ± 0.06 | 10.81 ± 0.05 | 10.72 ± 0.12 | 10.44 ± 0.15  | 10.95 ± 0.19  | 10.85 ± 0.05  | 10.89 ± 0.06 |
| PrimReturnValue             | 5.79 ± 0.17  | 5.54 ± 0.15  | 5.36 ± 0.15  | 5.29 ± 0.15  | 5.32 ± 0.15   | 5.39 ± 0.15   | 5.60 ± 0.16   | 5.42 ± 0.15  |
| PrimSubtract                | 11.59 ± 0.13 | 11.68 ± 0.06 | 11.15 ± 0.11 | 10.88 ± 0.16 | 11.81 ± 0.10  | 11.31 ± 0.19  | 11.85 ± 0.09  | 11.66 ± 0.12 |
| PrimVariableDeclare         | 3.45 ± 0.15  | 3.46 ± 0.17  | 3.25 ± 0.16  | 3.17 ± 0.15  | 3.09 ± 0.15   | 3.05 ± 0.15   | 3.33 ± 0.15   | 3.05 ± 0.15  |
| PrimVariableDeclareAssign   | 10.64 ± 0.15 | 10.40 ± 0.15 | 10.58 ± 0.19 | 10.37 ± 0.15 | 10.36 ± 0.15  | 10.41 ± 0.15  | 10.30 ± 0.15  | 10.37 ± 0.16 |
| PrimVariableInplaceAdd      | 10.16 ± 0.04 | 10.56 ± 0.04 | 10.16 ± 0.13 | 10.86 ± 0.06 | 10.70 ± 0.05  | 10.79 ± 0.09  | 10.69 ± 0.05  | 10.36 ± 0.07 |
| PrimVariableInplaceDivide   | 12.40 ± 0.06 | 12.57 ± 0.04 | 17.35 ± 0.14 | 13.14 ± 0.05 | 12.24 ± 0.07  | 12.66 ± 0.05  | 17.32 ± 0.04  | 12.62 ± 0.13 |
| PrimVariableInplaceMultiply | 11.01 ± 0.04 | 11.12 ± 0.03 | 11.15 ± 0.14 | 11.60 ± 0.05 | 11.25 ± 0.04  | 11.52 ± 0.05  | 11.28 ± 0.04  | 11.33 ± 0.08 |
| PrimVariableInplaceSubtract | 10.47 ± 0.06 | 10.58 ± 0.11 | 10.16 ± 0.13 | 10.96 ± 0.05 | 10.62 ± 0.05  | 10.80 ± 0.05  | 10.97 ± 0.17  | 10.50 ± 0.07 |
| PrimVariablePostfixDec      | 9.95 ± 0.03  | 10.53 ± 0.04 | 10.17 ± 0.13 | 10.86 ± 0.05 | 10.73 ± 0.19  | 10.59 ± 0.05  | 10.80 ± 0.05  | 10.29 ± 0.08 |
| PrimVariablePostfixInc      | 10.00 ± 0.04 | 10.63 ± 0.05 | 10.12 ± 0.24 | 10.66 ± 0.05 | 10.45 ± 0.11  | 10.54 ± 0.05  | 10.96 ± 0.06  | 10.30 ± 0.12 |
| PrimVariablePrefixDec       | 10.07 ± 0.04 | 10.68 ± 0.08 | 9.73 ± 0.15  | 10.60 ± 0.05 | 9.79 ± 0.06   | 10.68 ± 0.05  | 10.81 ± 0.05  | 10.62 ± 0.09 |
| PrimVariablePrefixInc       | 10.59 ± 0.17 | 11.15 ± 0.05 | 10.52 ± 0.14 | 10.95 ± 0.16 | 9.80 ± 0.08   | 11.19 ± 0.09  | 11.55 ± 0.12  | 10.04 ± 0.06 |

In [Table 4](#table4), we see that certain operations are more expensive for fixed-point than floating point primitives, but there are only small differences between 32 and 64 bits within each primitive class.

### <a name="table4">Table 4</a>: Mean CPU times for floating and fixed point primitive opcodes (+/- std error)
| Benchmark (100 reps)        | Fixed128 (ns)   | Fixed32 (ns)   | Fixed64 (ns)   | Float32 (ns)   | Float64 (ns)   |
|-----------------------------|-----------------|----------------|----------------|----------------|----------------|
| PrimAdd                     | 58.71 ± 0.13    | 13.07 ± 0.17   | 13.51 ± 0.08   | 11.87 ± 0.12   | 12.03 ± 0.09   |
| PrimDivide                  | 1206.46 ± 0.57  | 30.87 ± 0.06   | 32.16 ± 0.08   | 14.26 ± 0.08   | 14.05 ± 0.05   |
| PrimEqual                   | 18.73 ± 0.19    | 14.14 ± 0.06   | 13.84 ± 0.11   | 15.97 ± 0.09   | 14.77 ± 0.09   |
| PrimGreaterThan             | 18.54 ± 0.17    | 15.16 ± 0.06   | 14.73 ± 0.08   | 16.50 ± 0.25   | 15.53 ± 0.08   |
| PrimGreaterThanOrEqual      | 47.02 ± 0.14    | 15.18 ± 0.07   | 14.51 ± 0.08   | 15.42 ± 0.09   | 15.21 ± 0.06   |
| PrimLessThan                | 19.94 ± 0.10    | 15.33 ± 0.06   | 14.12 ± 0.08   | 15.71 ± 0.09   | 14.90 ± 0.06   |
| PrimLessThanOrEqual         | 21.50 ± 0.13    | 15.59 ± 0.16   | 14.85 ± 0.16   | 15.62 ± 0.08   | 15.14 ± 0.07   |
| PrimMultiply                | 87.94 ± 0.13    | 16.08 ± 0.07   | 15.25 ± 0.08   | 14.74 ± 0.08   | 14.43 ± 0.05   |
| PrimNegate                  | 23.17 ± 0.12    | 5.28 ± 0.07    | 4.67 ± 0.08    | 6.45 ± 0.13    | 5.00 ± 0.12    |
| PrimNotEqual                | 19.26 ± 0.17    | 14.13 ± 0.05   | 13.71 ± 0.08   | 15.58 ± 0.08   | 14.91 ± 0.16   |
| PrimPopToVariable           | 17.38 ± 0.07    | 8.99 ± 0.06    | 9.65 ± 0.08    | 9.52 ± 0.09    | 9.15 ± 0.04    |
| PrimPushConst               | 25.22 ± 0.19    | 8.75 ± 0.15    | 8.74 ± 0.15    | 8.64 ± 0.15    | 8.69 ± 0.15    |
| PrimPushVariable            | 15.96 ± 0.13    | 10.84 ± 0.04   | 11.38 ± 0.07   | 10.51 ± 0.08   | 10.77 ± 0.04   |
| PrimReturnValue             | 20.31 ± 0.15    | 5.31 ± 0.15    | 5.49 ± 0.15    | 5.83 ± 0.15    | 5.51 ± 0.15    |
| PrimSubtract                | 59.41 ± 0.19    | 12.78 ± 0.06   | 13.37 ± 0.14   | 12.16 ± 0.08   | 11.75 ± 0.04   |
| PrimVariableDeclare         | 6.53 ± 0.15     | 3.27 ± 0.15    | 3.18 ± 0.15    | 3.41 ± 0.16    | 3.32 ± 0.17    |
| PrimVariableDeclareAssign   | 28.78 ± 0.16    | 10.72 ± 0.16   | 10.49 ± 0.16   | 10.31 ± 0.15   | 10.51 ± 0.15   |
| PrimVariableInplaceAdd      | 44.50 ± 0.11    | 11.56 ± 0.07   | 14.02 ± 0.07   | 11.21 ± 0.06   | 10.38 ± 0.05   |
| PrimVariableInplaceDivide   | 1199.34 ± 1.63  | 29.78 ± 0.09   | 32.56 ± 0.10   | 11.81 ± 0.04   | 12.51 ± 0.05   |
| PrimVariableInplaceMultiply | 72.70 ± 0.09    | 14.83 ± 0.22   | 15.35 ± 0.07   | 11.39 ± 0.05   | 11.26 ± 0.10   |
| PrimVariableInplaceSubtract | 44.21 ± 0.09    | 11.97 ± 0.07   | 13.46 ± 0.07   | 11.30 ± 0.07   | 10.60 ± 0.05   |

## Math function opcodes

Similarly, we tested the math function opcodes for the various primitives. [Table 5](#table5) shows that most math function opcodes take significantly longer for fixed point than floating point primitives.

### <a name="table5">Table 5</a>: Math function opcodes for fixed and floating point primitives
| Benchmark (100 reps)   | Fixed128 (ns)   | Fixed32 (ns)   | Fixed64 (ns)   | Float32 (ns)   | Float64 (ns)   |
|------------------------|-----------------|----------------|----------------|----------------|----------------|
| MathAbs                | 89.46 ± 0.09    | 10.52 ± 0.05   | 10.99 ± 0.08   | 10.13 ± 0.09   | 9.73 ± 0.12    |
| MathAcos               | 3265.41 ± 1.60  | 166.07 ± 0.22  | 207.30 ± 0.10  | 21.48 ± 0.08   | 24.83 ± 0.14   |
| MathAcosh              | 10542.94 ± 7.31 | 290.25 ± 0.14  | 423.92 ± 0.89  | 37.18 ± 0.09   | 33.88 ± 0.08   |
| MathAsin               | 3182.39 ± 4.72  | 154.24 ± 0.07  | 194.32 ± 0.09  | 19.41 ± 0.08   | 21.66 ± 0.06   |
| MathAsinh              | 10639.94 ± 5.67 | 290.67 ± 0.58  | 408.54 ± 0.31  | 41.12 ± 0.40   | 38.73 ± 0.06   |
| MathAtan               | 2385.80 ± 1.28  | 88.90 ± 0.26   | 106.87 ± 0.08  | 25.40 ± 0.08   | 18.57 ± 0.06   |
| MathAtanh              | 6851.21 ± 4.00  | 175.82 ± 0.18  | 278.74 ± 0.22  | 37.21 ± 0.13   | 33.56 ± 0.07   |
| MathCos                | 5358.14 ± 3.57  | 141.72 ± 0.08  | 182.73 ± 0.12  | 11.81 ± 0.09   | 21.98 ± 0.16   |
| MathCosh               | 9598.22 ± 16.73 | 242.32 ± 0.17  | 344.26 ± 0.12  | 18.53 ± 0.46   | 24.14 ± 0.10   |
| MathExp                | 3933.70 ± 2.13  | 105.64 ± 0.18  | 143.16 ± 0.09  | 24.43 ± 0.11   | 22.27 ± 0.07   |
| MathPow                | --              | 331.40 ± 0.23  | 495.57 ± 0.61  | 33.69 ± 0.09   | 72.75 ± 0.07   |
| MathSin                | 5525.85 ± 3.15  | 145.46 ± 0.84  | 186.22 ± 0.10  | 12.25 ± 0.08   | 23.16 ± 0.10   |
| MathSinh               | 9496.59 ± 6.41  | 241.61 ± 0.09  | 344.71 ± 0.47  | 47.11 ± 0.09   | 39.48 ± 0.12   |
| MathSqrt               | 4551.67 ± 8.08  | 121.04 ± 0.10  | 194.51 ± 0.17  | 10.15 ± 0.09   | 9.55 ± 0.07    |
| MathTan                | 4074.97 ± 2.67  | 112.98 ± 0.12  | 135.75 ± 0.35  | 23.03 ± 0.14   | 20.76 ± 0.06   |
| MathTanh               | 11025.04 ± 4.33 | 260.51 ± 0.19  | 382.05 ± 0.25  | 36.16 ± 0.12   | 33.02 ± 0.17   |

# Parameter-dependent opcodes

Next, we profile the operations that may depend on variable size. When the relationship is linear, we report the least-squares linear fit, anchored to the first data point to ensure reliable estimates for small parameter values. For now, we do not attempt to do any nonlinear fitting.

## String-based opcodes

### <a name="table6">Table 6</a>: Least-squares linear fit parameters for string-based opcodes
| Benchmark (100 reps)           |   Slope (ns/char) |   Intercept (ns) |   Mean (ns) |   Std error (ns) | Net opcodes   |
|--------------------------------|-------------------|------------------|-------------|------------------|---------------|
| ObjectAddString                |             1.128 |           38.747 |   13742.782 |           14.243 | [8, 49]       |
| PushString                     |             0.545 |           44.120 |    4516.498 |            8.131 | [6, 14]       |
| VariableDeclareAssignString    |             0.543 |           51.653 |    4514.937 |            8.103 | [6, 2]        |
| ObjectGreaterThanString        |             0.017 |           13.284 |    4616.937 |            8.243 | [8, 43]       |
| ObjectEqualString              |             0.016 |           13.751 |    4601.948 |            8.227 | [8, 35]       |
| ObjectLessThanOrEqualString    |             0.014 |           15.059 |    4599.461 |            8.199 | [8, 41]       |
| ObjectLessThanString           |             0.013 |           14.206 |    4588.273 |            8.195 | [8, 39]       |
| ObjectGreaterThanOrEqualString |             0.013 |           13.627 |    4584.418 |            8.197 | [8, 45]       |
| ObjectNotEqualString           |             0.012 |           15.190 |    4582.888 |            8.191 | [8, 37]       |
| PushVariableString             |            -0.000 |            5.019 |    4471.781 |            8.101 | [8, 14]       |

<p align="center"><img src="https://lh3.googleusercontent.com/x_2cbgzSlwBvWMeHQitI_21loITfcWKTzq483LRI75a8dTbaFHImrY7nODcruph3Ydj92NLs4y8" width="600"/></p>
<p align="center"><img src="https://lh3.googleusercontent.com/uVMF38lLDQ556126BMMsxfONlATjW_0T1B049w0NrLUoPQ4gSzgz8E0attdGjGJxqQijD9UXetg" width="600"/></p>
<p align="center"><img src="https://lh3.googleusercontent.com/MIckMhq_X9IiNgrk3sWsJ-_kf0-Atz5A4jDMqpHaiq2AWal3w9i0VpdI0dy_PleNPCAk7idXzbM" width="600"/></p>
<p align="center"><img src="https://lh3.googleusercontent.com/yPCTvd5Osq2mEFfbhwcu9DCtnDRZjgVFb2ilPtEmFQeYIUguJypx5Vfp0qRnaF3bPwkaAhabIkI" width="600"/></p>
<p align="center"><img src="https://lh3.googleusercontent.com/JMGez-WoeUzB8-7TfSIaL7IqHc65DlyWusBKb7tbB3wDM_AwzwZ4jSEbQMpjEZE6TG3CAj12WKM" width="600"/></p>
<p align="center"><img src="https://lh3.googleusercontent.com/dmPd8p11ANh3q3nUV7-Dy9Bk00PP3C4XdHb6HDn6SVx2YIPbsdguNfZVny_R2QK6IuRXu554RUU" width="600"/></p>
<p align="center"><img src="https://lh3.googleusercontent.com/XtZHuq-gM385i0I2Sk56DgA8CQKujQR7dF_4wHOCxR-udr3rsk8QaquDFp1os7ux-0LOynTl1Fc" width="600"/></p>
<p align="center"><img src="https://lh3.googleusercontent.com/ULrd2GvoXDK7Zpa5-Gm1B-L99KMJTApA5sWxpg0wQ2kMhxGqSdtnROvzH4FawsGi9_-JI_USyBs" width="600"/></p>
<p align="center"><img src="https://lh3.googleusercontent.com/tjAvEXAplIo5tjCpUs9-7rJ8gGB_5wmhsdHuhMxtn1IoDI7gWhQcn13z6OYXWOhvitcT3YAPhMg" width="600"/></p>

## Array-based opcodes

We use a similar approach to profile array operations. As shown below, there is often a change in slope or behavior once the array length exceeds about 6000. For this reason, we compute the least-squares fits based only on arrays smaller than 6000. The change in behavior is likely a result of machine-dependent memory management, and memory storage will probably be included separately in transaction pricing.

### <a name="table7">Table 7</a>: Least-squares linear fit parameters for array-based opcodes
| Benchmark (100 reps)   |   Slope (ns/char) |   Intercept (ns) |   Mean (ns) |   Std error (ns) | Net opcodes          |
|------------------------|-------------------|------------------|-------------|------------------|----------------------|
| ReverseArray           |             0.277 |           13.673 |    2847.449 |            5.536 | [8, 107]             |
| ExtendArray            |             0.201 |           33.260 |    2967.516 |            6.909 | [8, 8, 102]          |
| DeclareTwoArray        |             0.126 |          114.496 |    1339.739 |            3.649 | [7, 98, 2, 7, 98, 2] |
| AppendArray            |             0.091 |           24.856 |    1351.099 |            3.797 | [8, 7, 99]           |
| DeclareArray           |             0.045 |           47.877 |     625.744 |            2.661 | [7, 98, 2]           |
| PopFrontArray          |             0.041 |           21.299 |    1058.931 |            3.596 | [8, 105, 14]         |
| AssignArray            |             0.003 |            7.606 |     600.269 |            2.648 | [8, 7, 7, 109]       |
| EraseArray             |             0.001 |            7.806 |     592.606 |            2.645 | [8, 7, 101]          |
| PopBackArray           |            -0.001 |           22.580 |     594.415 |            2.659 | [8, 103, 14]         |
| CountArray             |            -0.001 |           15.849 |     591.189 |            2.673 | [8, 100, 14]         |

<p align="center"><img src="https://lh3.googleusercontent.com/UfiJ1SiH6oGEjyFpY40euuiSFIeKPikstWsH_DrBCv3wMj58-FSqUJN2wz_7fFNuf6WFbm311Tc" width="600"/></p>
<p align="center"><img src="https://lh3.googleusercontent.com/HF2r6H2U6hBM2M5TSI-CYYZuThxEsVfq--fX3W1LN6gHqWmqkA0HAsOEYm88I_UKmF28rQ8Bu7s" width="600"/></p>
<p align="center"><img src="https://lh3.googleusercontent.com/iSrgeIvhH7qt5wRdbcjtQtP2TU5gDXFu0lJzTQJLMPUsPiaTflURRSLKLFRjc4byj4-sVdUcLFc" width="600"/></p>
<p align="center"><img src="https://lh3.googleusercontent.com/B0AwMC-Ee00bKHMg42lFOt6GRScOzDlzmVNSr6iL88gnFPwZWvqPunaB1hLC-uLs9OpYp0259Tk" width="600"/></p>
<p align="center"><img src="https://lh3.googleusercontent.com/fBZPSicVkzb41U3vMOw3uQrEbFyMaewhP7tn2p06ieMqHFaa8tu4GuncesQo_fcwa7SUnv_cLSA" width="600"/></p>
<p align="center"><img src="https://lh3.googleusercontent.com/_QHkdsqYvNsv4d7yX7PEL1ATEyMSAUtZe0Bwod3vksAc7kuLwKY7EFGPKV1RC4biJ9V0D-u4mHA" width="600"/></p>

## Tensor-based opcodes

We profile tensor-based operations using a similar approach. To ensure that they do not depend on the dimension of the tensor, we tested the following dimensions independently: 2, 3 and 4.

### <a name="table8">Table 8</a>: Least-squares linear fit parameters for tensor-based opcodes
| Benchmark (100 reps)   |   Slope (ns/char) |   Intercept (ns) |    Mean (ns) |   Std error (ns) | Net opcodes                                                                   |
|------------------------|-------------------|------------------|--------------|------------------|-------------------------------------------------------------------------------|
| FromStrTensor_3        |            72.117 |          658.957 | 21459495.574 |          814.668 | [8, 6, 422]                                                                   |
| FromStrTensor_2        |            71.999 |          665.464 | 27505233.436 |          822.809 | [8, 6, 422]                                                                   |
| FromStrTensor_4        |            71.685 |          563.635 | 18579198.218 |          834.691 | [8, 6, 422]                                                                   |
| FillRandTensor_4       |            16.037 |           30.307 |  2099429.698 |          264.307 | [8, 416]                                                                      |
| FillRandTensor_3       |            16.025 |           30.315 |  2335978.656 |          256.090 | [8, 416]                                                                      |
| FillRandTensor_2       |            15.976 |           26.033 |  2954216.832 |          257.032 | [8, 416]                                                                      |
| FillTensor_3           |             1.077 |            7.318 |   183323.328 |           71.658 | [8, 7, 415]                                                                   |
| FillTensor_2           |             1.035 |           14.028 |   217779.776 |           69.744 | [8, 7, 415]                                                                   |
| FillTensor_4           |             1.008 |            6.782 |   157216.237 |           71.689 | [8, 7, 415]                                                                   |
| DeclareTensor_3        |             0.193 |          281.184 |    28487.991 |           27.967 | [7, 98, 2, 8, 7, 7, 109, 8, 7, 7, 109, 8, 7, 7, 109, 8, 402, 2]               |
| DeclareTensor_4        |             0.193 |          286.142 |    28007.845 |           28.103 | [7, 98, 2, 8, 7, 7, 109, 8, 7, 7, 109, 8, 7, 7, 109, 8, 7, 7, 109, 8, 402, 2] |
| DeclareTensor_2        |             0.154 |          277.480 |    28623.532 |           25.013 | [7, 98, 2, 8, 7, 7, 109, 8, 7, 7, 109, 8, 402, 2]                             |
| SizeTensor_4           |             0.002 |           15.864 |    27822.580 |           28.271 | [8, 419, 14]                                                                  |
| SizeTensor_2           |             0.002 |           26.582 |    28530.266 |           25.250 | [8, 419, 14]                                                                  |
| SizeTensor_3           |            -0.001 |           15.156 |    28087.432 |           27.902 | [8, 419, 14]                                                                  |

<p align="center"><img src="https://lh3.googleusercontent.com/g8_zO9xpEmQ9Yc18sF8pQQ1twbFbiSrzIzMG78WO76IcNJC8TKnIoW86uRixEEyEhpyEK35mtpM" width="600"/></p>
<p align="center"><img src="https://lh3.googleusercontent.com/wU7uOueyDmyWTef0S0oBmrLqlw0IL_VTpciNQpPMv_LmzKPcd4scQG3AkB3uOIU458_Kcns4Ibw" width="600"/></p>
<p align="center"><img src="https://lh3.googleusercontent.com/sYVW4_KXe3bus6wj4JmNxTyow3qY05HNfuKuu4S-DnKb3KK9QBPXwplGser29NhdolYrLGY5n1diWD4deNizoYLF5N31fGtMr0o23-HcyqWIvelB7AnvzRZn64If7mh7U_E3Fr6f=w2400" width="600"/></p>
<p align="center"><img src="https://lh3.googleusercontent.com/awDzFVNDZbiNk2avZTqJHfW_8wNZdsZE3eewJ4mWm3Fw83IG234LeDFqswvthmxiJLodtxgUjTY" width="600"/></p>

## Crypto-based opcodes

### <a name="table9">Table 9</a>: Least-squares linear fit parameters for crypto-based opcodes
| Benchmark (100 reps)   |   Slope (ns/char) |   Intercept (ns) |   Mean (ns) |   Std error (ns) | Net opcodes         |
|------------------------|-------------------|------------------|-------------|------------------|---------------------|
| Sha256UpdateStr        |             4.576 |           63.464 |   37633.507 |           23.537 | [8, 6, 235]         |
| Sha256DeclareBuf       |             0.012 |           31.005 |     150.590 |            1.061 | [7, 219]            |
| Sha256Final            |             0.000 |          501.583 |     511.842 |            0.418 | [8, 237, 14]        |
| Sha256Reset            |             0.000 |           29.546 |      36.277 |            0.387 | [8, 238]            |
| Sha256UpdateBuf        |            -0.000 |          202.819 |     316.383 |            1.077 | [233, 2, 8, 6, 235] |

<p align="center"><img src="https://lh3.googleusercontent.com/1VsZkBgZNNWg6G6RoTrFbiZKzADiKz3aDkYt0DyZgI_tR7ZSS68WnsPeeCNgEzkLOLW-npZSiPk" width="600"/></p>
<p align="center"><img src="https://lh3.googleusercontent.com/ZomW7d8kat5ZL7XxBPfboXujHe0yGAkuyVJnOs5RRsHW5ef08aEmNY6YkFIhU4xTqYAhd9rBLlA" width="600"/></p>

## <a name="app">Appendix</a>:  Parameter-independent opcode tables

|   Opcode (Base) | Name                          |   Estimated time (ns) |
|-----------------|-------------------------------|-----------------------|
|               1 | LocalVariableDeclare          |                  8.90 |
|               4 | PushFalse                     |                 14.72 |
|               5 | PushTrue                      |                  3.62 |
|               7 | PushConstant                  |                 13.71 |
|              14 | Discard                       |                  7.48 |
|              15 | Destruct                      |                  2.27 |
|              18 | Jump                          |                  0.00 |
|              19 | JumpIfFalse                   |                  6.09 |
|              21 | Return                        |                 31.76 |
|              23 | ForRangeInit                  |                  6.78 |
|              24 | ForRangeIterate               |                  6.78 |
|              25 | ForRangeTerminate             |                  6.78 |
|              26 | InvokeUserDefinedFreeFunction |                 18.95 |
|              31 | JumpIfFalseOrPop              |                  5.08 |
|              32 | JumpIfTrueOrPop               |                  2.17 |
|              33 | Not                           |                  4.14 |

|   Opcode (Fixed128) | Name                               |   Estimated time (ns) |
|---------------------|------------------------------------|-----------------------|
|                   2 | LocalVariableDeclareAssign         |                 11.04 |
|                   8 | PushLocalVariable                  |                  4.92 |
|                   9 | PopToLocalVariable                 |                 12.46 |
|                  22 | ReturnValue                        |                 34.33 |
|                  35 | ObjectEqual                        |                 13.81 |
|                  37 | ObjectNotEqual                     |                 14.34 |
|                  39 | ObjectLessThan                     |                 15.02 |
|                  41 | ObjectLessThanOrEqual              |                 16.58 |
|                  43 | ObjectGreaterThan                  |                 13.62 |
|                  45 | ObjectGreaterThanOrEqual           |                 42.10 |
|                  47 | ObjectNegate                       |                 23.17 |
|                  49 | ObjectAdd                          |                 53.79 |
|                  53 | LocalVariableObjectInplaceAdd      |                 39.58 |
|                  56 | ObjectSubtract                     |                 54.49 |
|                  60 | LocalVariableObjectInplaceSubtract |                 39.29 |
|                  63 | ObjectMultiply                     |                 83.02 |
|                  67 | LocalVariableObjectInplaceMultiply |                 67.78 |
|                  70 | ObjectDivide                       |               1201.54 |
|                  74 | LocalVariableObjectInplaceDivide   |               1194.42 |
|                  81 | PushLargeConstant                  |                 20.07 |
|                 320 | ::abs^Fixed128^Fixed128            |                 89.46 |
|                 325 | ::exp^Fixed128^Fixed128            |               3933.70 |
|                 358 | ::sqrt^Fixed128^Fixed128           |               4551.67 |
|                 411 | ::sin^Fixed128^Fixed128            |               5525.85 |
|                 412 | ::cos^Fixed128^Fixed128            |               5358.14 |
|                 413 | ::tan^Fixed128^Fixed128            |               4074.97 |
|                 414 | ::asin^Fixed128^Fixed128           |               3182.39 |
|                 415 | ::acos^Fixed128^Fixed128           |               3265.41 |
|                 416 | ::atan^Fixed128^Fixed128           |               2385.80 |
|                 418 | ::sinh^Fixed128^Fixed128           |               9496.59 |
|                 419 | ::cosh^Fixed128^Fixed128           |               9598.22 |
|                 420 | ::tanh^Fixed128^Fixed128           |              11025.04 |
|                 421 | ::asinh^Fixed128^Fixed128          |              10639.94 |
|                 422 | ::acosh^Fixed128^Fixed128          |              10542.94 |
|                 423 | ::atanh^Fixed128^Fixed128          |               6851.21 |

|   Opcode (Fixed32) | Name                                  |   Estimated time (ns) |
|--------------------|---------------------------------------|-----------------------|
|                  2 | LocalVariableDeclareAssign            |                  0.00 |
|                  8 | PushLocalVariable                     |                  0.73 |
|                  9 | PopToLocalVariable                    |                  7.59 |
|                 22 | ReturnValue                           |                 25.69 |
|                 34 | PrimitiveEqual                        |                 13.40 |
|                 36 | PrimitiveNotEqual                     |                 13.40 |
|                 38 | PrimitiveLessThan                     |                 14.59 |
|                 40 | PrimitiveLessThanOrEqual              |                 14.86 |
|                 42 | PrimitiveGreaterThan                  |                 14.43 |
|                 44 | PrimitiveGreaterThanOrEqual           |                 14.44 |
|                 46 | PrimitiveNegate                       |                  5.28 |
|                 48 | PrimitiveAdd                          |                 12.33 |
|                 52 | LocalVariablePrimitiveInplaceAdd      |                 10.16 |
|                 55 | PrimitiveSubtract                     |                 12.05 |
|                 59 | LocalVariablePrimitiveInplaceSubtract |                 10.57 |
|                 62 | PrimitiveMultiply                     |                 15.34 |
|                 66 | LocalVariablePrimitiveInplaceMultiply |                 13.43 |
|                 69 | PrimitiveDivide                       |                 30.14 |
|                 73 | LocalVariablePrimitiveInplaceDivide   |                 28.38 |
|                318 | ::abs^Fixed32^Fixed32                 |                 10.52 |
|                323 | ::exp^Fixed32^Fixed32                 |                105.64 |
|                343 | ::pow^Fixed32,Fixed32^Fixed32         |                330.67 |
|                356 | ::sqrt^Fixed32^Fixed32                |                121.04 |
|                385 | ::sin^Fixed32^Fixed32                 |                145.46 |
|                386 | ::cos^Fixed32^Fixed32                 |                141.72 |
|                387 | ::tan^Fixed32^Fixed32                 |                112.98 |
|                388 | ::asin^Fixed32^Fixed32                |                154.24 |
|                389 | ::acos^Fixed32^Fixed32                |                166.07 |
|                390 | ::atan^Fixed32^Fixed32                |                 88.90 |
|                392 | ::sinh^Fixed32^Fixed32                |                241.61 |
|                393 | ::cosh^Fixed32^Fixed32                |                242.32 |
|                394 | ::tanh^Fixed32^Fixed32                |                260.51 |
|                395 | ::asinh^Fixed32^Fixed32               |                290.67 |
|                396 | ::acosh^Fixed32^Fixed32               |                290.25 |
|                397 | ::atanh^Fixed32^Fixed32               |                175.82 |

|   Opcode (Fixed64) | Name                                  |   Estimated time (ns) |
|--------------------|---------------------------------------|-----------------------|
|                  2 | LocalVariableDeclareAssign            |                  0.00 |
|                  8 | PushLocalVariable                     |                  1.26 |
|                  9 | PopToLocalVariable                    |                  7.50 |
|                 22 | ReturnValue                           |                 25.87 |
|                 34 | PrimitiveEqual                        |                 12.58 |
|                 36 | PrimitiveNotEqual                     |                 12.45 |
|                 38 | PrimitiveLessThan                     |                 12.86 |
|                 40 | PrimitiveLessThanOrEqual              |                 13.60 |
|                 42 | PrimitiveGreaterThan                  |                 13.47 |
|                 44 | PrimitiveGreaterThanOrEqual           |                 13.25 |
|                 46 | PrimitiveNegate                       |                  4.67 |
|                 48 | PrimitiveAdd                          |                 12.25 |
|                 52 | LocalVariablePrimitiveInplaceAdd      |                 11.87 |
|                 55 | PrimitiveSubtract                     |                 12.11 |
|                 59 | LocalVariablePrimitiveInplaceSubtract |                 11.31 |
|                 62 | PrimitiveMultiply                     |                 13.99 |
|                 66 | LocalVariablePrimitiveInplaceMultiply |                 13.20 |
|                 69 | PrimitiveDivide                       |                 30.90 |
|                 73 | LocalVariablePrimitiveInplaceDivide   |                 30.41 |
|                319 | ::abs^Fixed64^Fixed64                 |                 10.99 |
|                324 | ::exp^Fixed64^Fixed64                 |                143.16 |
|                344 | ::pow^Fixed64,Fixed64^Fixed64         |                494.31 |
|                357 | ::sqrt^Fixed64^Fixed64                |                194.51 |
|                398 | ::sin^Fixed64^Fixed64                 |                186.22 |
|                399 | ::cos^Fixed64^Fixed64                 |                182.73 |
|                400 | ::tan^Fixed64^Fixed64                 |                135.75 |
|                401 | ::asin^Fixed64^Fixed64                |                194.32 |
|                402 | ::acos^Fixed64^Fixed64                |                207.30 |
|                403 | ::atan^Fixed64^Fixed64                |                106.87 |
|                405 | ::sinh^Fixed64^Fixed64                |                344.71 |
|                406 | ::cosh^Fixed64^Fixed64                |                344.26 |
|                407 | ::tanh^Fixed64^Fixed64                |                382.05 |
|                408 | ::asinh^Fixed64^Fixed64               |                408.54 |
|                409 | ::acosh^Fixed64^Fixed64               |                423.92 |
|                410 | ::atanh^Fixed64^Fixed64               |                278.74 |

|   Opcode (Float32) | Name                                  |   Estimated time (ns) |
|--------------------|---------------------------------------|-----------------------|
|                  2 | LocalVariableDeclareAssign            |                  0.00 |
|                  8 | PushLocalVariable                     |                  0.29 |
|                  9 | PopToLocalVariable                    |                  8.16 |
|                 22 | ReturnValue                           |                 26.21 |
|                 34 | PrimitiveEqual                        |                 15.68 |
|                 36 | PrimitiveNotEqual                     |                 15.29 |
|                 38 | PrimitiveLessThan                     |                 15.42 |
|                 40 | PrimitiveLessThanOrEqual              |                 15.33 |
|                 42 | PrimitiveGreaterThan                  |                 16.21 |
|                 44 | PrimitiveGreaterThanOrEqual           |                 15.13 |
|                 46 | PrimitiveNegate                       |                  6.45 |
|                 48 | PrimitiveAdd                          |                 11.58 |
|                 52 | LocalVariablePrimitiveInplaceAdd      |                  9.85 |
|                 55 | PrimitiveSubtract                     |                 11.87 |
|                 59 | LocalVariablePrimitiveInplaceSubtract |                  9.94 |
|                 62 | PrimitiveMultiply                     |                 14.45 |
|                 66 | LocalVariablePrimitiveInplaceMultiply |                 10.03 |
|                 69 | PrimitiveDivide                       |                 13.98 |
|                 73 | LocalVariablePrimitiveInplaceDivide   |                 10.44 |
|                316 | ::abs^Float32^Float32                 |                 10.13 |
|                321 | ::exp^Float32^Float32                 |                 24.43 |
|                341 | ::pow^Float32,Float32^Float32         |                 33.40 |
|                354 | ::sqrt^Float32^Float32                |                 10.15 |
|                359 | ::sin^Float32^Float32                 |                 12.25 |
|                360 | ::cos^Float32^Float32                 |                 11.81 |
|                361 | ::tan^Float32^Float32                 |                 23.03 |
|                362 | ::asin^Float32^Float32                |                 19.41 |
|                363 | ::acos^Float32^Float32                |                 21.48 |
|                364 | ::atan^Float32^Float32                |                 25.40 |
|                366 | ::sinh^Float32^Float32                |                 47.11 |
|                367 | ::cosh^Float32^Float32                |                 18.53 |
|                368 | ::tanh^Float32^Float32                |                 36.16 |
|                369 | ::asinh^Float32^Float32               |                 41.12 |
|                370 | ::acosh^Float32^Float32               |                 37.18 |
|                371 | ::atanh^Float32^Float32               |                 37.21 |

|   Opcode (Float64) | Name                                  |   Estimated time (ns) |
|--------------------|---------------------------------------|-----------------------|
|                  2 | LocalVariableDeclareAssign            |                  0.00 |
|                  8 | PushLocalVariable                     |                  0.61 |
|                  9 | PopToLocalVariable                    |                  7.67 |
|                 22 | ReturnValue                           |                 25.89 |
|                 34 | PrimitiveEqual                        |                 14.17 |
|                 36 | PrimitiveNotEqual                     |                 14.30 |
|                 38 | PrimitiveLessThan                     |                 14.29 |
|                 40 | PrimitiveLessThanOrEqual              |                 14.53 |
|                 42 | PrimitiveGreaterThan                  |                 14.93 |
|                 44 | PrimitiveGreaterThanOrEqual           |                 14.61 |
|                 46 | PrimitiveNegate                       |                  5.00 |
|                 48 | PrimitiveAdd                          |                 11.42 |
|                 52 | LocalVariablePrimitiveInplaceAdd      |                  8.90 |
|                 55 | PrimitiveSubtract                     |                 11.15 |
|                 59 | LocalVariablePrimitiveInplaceSubtract |                  9.12 |
|                 62 | PrimitiveMultiply                     |                 13.82 |
|                 66 | LocalVariablePrimitiveInplaceMultiply |                  9.78 |
|                 69 | PrimitiveDivide                       |                 13.44 |
|                 73 | LocalVariablePrimitiveInplaceDivide   |                 11.03 |
|                317 | ::abs^Float64^Float64                 |                  9.73 |
|                322 | ::exp^Float64^Float64                 |                 22.27 |
|                342 | ::pow^Float64,Float64^Float64         |                 72.15 |
|                355 | ::sqrt^Float64^Float64                |                  9.55 |
|                372 | ::sin^Float64^Float64                 |                 23.16 |
|                373 | ::cos^Float64^Float64                 |                 21.98 |
|                374 | ::tan^Float64^Float64                 |                 20.76 |
|                375 | ::asin^Float64^Float64                |                 21.66 |
|                376 | ::acos^Float64^Float64                |                 24.83 |
|                377 | ::atan^Float64^Float64                |                 18.57 |
|                379 | ::sinh^Float64^Float64                |                 39.48 |
|                380 | ::cosh^Float64^Float64                |                 24.14 |
|                381 | ::tanh^Float64^Float64                |                 33.02 |
|                382 | ::asinh^Float64^Float64               |                 38.73 |
|                383 | ::acosh^Float64^Float64               |                 33.88 |
|                384 | ::atanh^Float64^Float64               |                 33.56 |

|   Opcode (Int16) | Name                                  |   Estimated time (ns) |
|------------------|---------------------------------------|-----------------------|
|                2 | LocalVariableDeclareAssign            |                  0.00 |
|                8 | PushLocalVariable                     |                  0.70 |
|                9 | PopToLocalVariable                    |                  7.89 |
|               22 | ReturnValue                           |                 25.72 |
|               27 | LocalVariablePrefixInc                |                  1.47 |
|               28 | LocalVariablePrefixDec                |                  1.70 |
|               29 | LocalVariablePostfixInc               |                  1.79 |
|               30 | LocalVariablePostfixDec               |                  1.74 |
|               34 | PrimitiveEqual                        |                 13.22 |
|               36 | PrimitiveNotEqual                     |                 13.46 |
|               38 | PrimitiveLessThan                     |                 13.52 |
|               40 | PrimitiveLessThanOrEqual              |                 13.65 |
|               42 | PrimitiveGreaterThan                  |                 13.43 |
|               44 | PrimitiveGreaterThanOrEqual           |                 13.14 |
|               46 | PrimitiveNegate                       |                  4.84 |
|               48 | PrimitiveAdd                          |                 11.21 |
|               52 | LocalVariablePrimitiveInplaceAdd      |                  8.84 |
|               55 | PrimitiveSubtract                     |                 11.02 |
|               59 | LocalVariablePrimitiveInplaceSubtract |                  8.94 |
|               62 | PrimitiveMultiply                     |                 14.13 |
|               66 | LocalVariablePrimitiveInplaceMultiply |                  9.54 |
|               69 | PrimitiveDivide                       |                 13.99 |
|               73 | LocalVariablePrimitiveInplaceDivide   |                 10.75 |

|   Opcode (Int32) | Name                                  |   Estimated time (ns) |
|------------------|---------------------------------------|-----------------------|
|                2 | LocalVariableDeclareAssign            |                  0.00 |
|                8 | PushLocalVariable                     |                  0.89 |
|                9 | PopToLocalVariable                    |                  7.55 |
|               22 | ReturnValue                           |                 25.88 |
|               27 | LocalVariablePrefixInc                |                  2.70 |
|               28 | LocalVariablePrefixDec                |                  2.23 |
|               29 | LocalVariablePostfixInc               |                  2.13 |
|               30 | LocalVariablePostfixDec               |                  2.10 |
|               34 | PrimitiveEqual                        |                 12.79 |
|               36 | PrimitiveNotEqual                     |                 12.94 |
|               38 | PrimitiveLessThan                     |                 13.04 |
|               40 | PrimitiveLessThanOrEqual              |                 13.18 |
|               42 | PrimitiveGreaterThan                  |                 13.34 |
|               44 | PrimitiveGreaterThanOrEqual           |                 13.21 |
|               46 | PrimitiveNegate                       |                  4.62 |
|               48 | PrimitiveAdd                          |                 10.68 |
|               52 | LocalVariablePrimitiveInplaceAdd      |                  8.72 |
|               55 | PrimitiveSubtract                     |                 10.69 |
|               59 | LocalVariablePrimitiveInplaceSubtract |                  8.90 |
|               62 | PrimitiveMultiply                     |                 13.26 |
|               66 | LocalVariablePrimitiveInplaceMultiply |                  9.35 |
|               69 | PrimitiveDivide                       |                 13.53 |
|               73 | LocalVariablePrimitiveInplaceDivide   |                 10.73 |

|   Opcode (Int64) | Name                                  |   Estimated time (ns) |
|------------------|---------------------------------------|-----------------------|
|                2 | LocalVariableDeclareAssign            |                  0.00 |
|                8 | PushLocalVariable                     |                  0.64 |
|                9 | PopToLocalVariable                    |                  7.76 |
|               22 | ReturnValue                           |                 25.78 |
|               27 | LocalVariablePrefixInc                |                  2.43 |
|               28 | LocalVariablePrefixDec                |                  2.12 |
|               29 | LocalVariablePostfixInc               |                  2.37 |
|               30 | LocalVariablePostfixDec               |                  2.07 |
|               34 | PrimitiveEqual                        |                 13.36 |
|               36 | PrimitiveNotEqual                     |                 13.22 |
|               38 | PrimitiveLessThan                     |                 12.93 |
|               40 | PrimitiveLessThanOrEqual              |                 13.35 |
|               42 | PrimitiveGreaterThan                  |                 13.43 |
|               44 | PrimitiveGreaterThanOrEqual           |                 13.06 |
|               46 | PrimitiveNegate                       |                  5.20 |
|               48 | PrimitiveAdd                          |                 11.20 |
|               52 | LocalVariablePrimitiveInplaceAdd      |                  8.81 |
|               55 | PrimitiveSubtract                     |                 10.92 |
|               59 | LocalVariablePrimitiveInplaceSubtract |                  8.73 |
|               62 | PrimitiveMultiply                     |                 13.65 |
|               66 | LocalVariablePrimitiveInplaceMultiply |                  9.57 |
|               69 | PrimitiveDivide                       |                 17.44 |
|               73 | LocalVariablePrimitiveInplaceDivide   |                 15.67 |

|   Opcode (Int8) | Name                                  |   Estimated time (ns) |
|-----------------|---------------------------------------|-----------------------|
|               2 | LocalVariableDeclareAssign            |                  0.00 |
|               8 | PushLocalVariable                     |                  0.57 |
|               9 | PopToLocalVariable                    |                  7.57 |
|              22 | ReturnValue                           |                 25.72 |
|              27 | LocalVariablePrefixInc                |                  1.57 |
|              28 | LocalVariablePrefixDec                |                  2.12 |
|              29 | LocalVariablePostfixInc               |                  2.12 |
|              30 | LocalVariablePostfixDec               |                  2.09 |
|              34 | PrimitiveEqual                        |                 14.73 |
|              36 | PrimitiveNotEqual                     |                 13.94 |
|              38 | PrimitiveLessThan                     |                 13.81 |
|              40 | PrimitiveLessThanOrEqual              |                 13.96 |
|              42 | PrimitiveGreaterThan                  |                 13.62 |
|              44 | PrimitiveGreaterThanOrEqual           |                 13.65 |
|              46 | PrimitiveNegate                       |                  5.28 |
|              48 | PrimitiveAdd                          |                  9.66 |
|              52 | LocalVariablePrimitiveInplaceAdd      |                  8.94 |
|              55 | PrimitiveSubtract                     |                 10.53 |
|              59 | LocalVariablePrimitiveInplaceSubtract |                  9.06 |
|              62 | PrimitiveMultiply                     |                 13.44 |
|              66 | LocalVariablePrimitiveInplaceMultiply |                  9.89 |
|              69 | PrimitiveDivide                       |                 13.74 |
|              73 | LocalVariablePrimitiveInplaceDivide   |                 11.49 |

|   Opcode (UInt16) | Name                                  |   Estimated time (ns) |
|-------------------|---------------------------------------|-----------------------|
|                 2 | LocalVariableDeclareAssign            |                  0.00 |
|                 8 | PushLocalVariable                     |                  0.70 |
|                 9 | PopToLocalVariable                    |                  7.77 |
|                22 | ReturnValue                           |                 25.70 |
|                27 | LocalVariablePrefixInc                |                  1.30 |
|                28 | LocalVariablePrefixDec                |                  1.29 |
|                29 | LocalVariablePostfixInc               |                  1.95 |
|                30 | LocalVariablePostfixDec               |                  2.23 |
|                34 | PrimitiveEqual                        |                 13.32 |
|                36 | PrimitiveNotEqual                     |                 13.51 |
|                38 | PrimitiveLessThan                     |                 13.31 |
|                40 | PrimitiveLessThanOrEqual              |                 13.96 |
|                42 | PrimitiveGreaterThan                  |                 13.43 |
|                44 | PrimitiveGreaterThanOrEqual           |                 13.25 |
|                46 | PrimitiveNegate                       |                  5.05 |
|                48 | PrimitiveAdd                          |                 11.26 |
|                52 | LocalVariablePrimitiveInplaceAdd      |                  8.98 |
|                55 | PrimitiveSubtract                     |                 11.11 |
|                59 | LocalVariablePrimitiveInplaceSubtract |                  8.90 |
|                62 | PrimitiveMultiply                     |                 14.12 |
|                66 | LocalVariablePrimitiveInplaceMultiply |                  9.53 |
|                69 | PrimitiveDivide                       |                 13.98 |
|                73 | LocalVariablePrimitiveInplaceDivide   |                 10.52 |

|   Opcode (UInt32) | Name                                  |   Estimated time (ns) |
|-------------------|---------------------------------------|-----------------------|
|                 2 | LocalVariableDeclareAssign            |                  0.00 |
|                 8 | PushLocalVariable                     |                  1.11 |
|                 9 | PopToLocalVariable                    |                  7.26 |
|                22 | ReturnValue                           |                 25.77 |
|                27 | LocalVariablePrefixInc                |                  2.74 |
|                28 | LocalVariablePrefixDec                |                  2.23 |
|                29 | LocalVariablePostfixInc               |                  2.09 |
|                30 | LocalVariablePostfixDec               |                  2.14 |
|                34 | PrimitiveEqual                        |                 12.47 |
|                36 | PrimitiveNotEqual                     |                 12.71 |
|                38 | PrimitiveLessThan                     |                 13.43 |
|                40 | PrimitiveLessThanOrEqual              |                 12.92 |
|                42 | PrimitiveGreaterThan                  |                 12.99 |
|                44 | PrimitiveGreaterThanOrEqual           |                 12.81 |
|                46 | PrimitiveNegate                       |                  4.65 |
|                48 | PrimitiveAdd                          |                 10.30 |
|                52 | LocalVariablePrimitiveInplaceAdd      |                  8.70 |
|                55 | PrimitiveSubtract                     |                 10.19 |
|                59 | LocalVariablePrimitiveInplaceSubtract |                  8.71 |
|                62 | PrimitiveMultiply                     |                 12.75 |
|                66 | LocalVariablePrimitiveInplaceMultiply |                  9.43 |
|                69 | PrimitiveDivide                       |                 13.08 |
|                73 | LocalVariablePrimitiveInplaceDivide   |                 10.57 |

|   Opcode (UInt64) | Name                                  |   Estimated time (ns) |
|-------------------|---------------------------------------|-----------------------|
|                 2 | LocalVariableDeclareAssign            |                  0.00 |
|                 8 | PushLocalVariable                     |                  0.56 |
|                 9 | PopToLocalVariable                    |                  8.23 |
|                22 | ReturnValue                           |                 25.98 |
|                27 | LocalVariablePrefixInc                |                  2.99 |
|                28 | LocalVariablePrefixDec                |                  2.25 |
|                29 | LocalVariablePostfixInc               |                  2.41 |
|                30 | LocalVariablePostfixDec               |                  2.24 |
|                34 | PrimitiveEqual                        |                 13.66 |
|                36 | PrimitiveNotEqual                     |                 13.46 |
|                38 | PrimitiveLessThan                     |                 13.38 |
|                40 | PrimitiveLessThanOrEqual              |                 13.67 |
|                42 | PrimitiveGreaterThan                  |                 13.67 |
|                44 | PrimitiveGreaterThanOrEqual           |                 13.33 |
|                46 | PrimitiveNegate                       |                  5.48 |
|                48 | PrimitiveAdd                          |                 11.32 |
|                52 | LocalVariablePrimitiveInplaceAdd      |                  9.06 |
|                55 | PrimitiveSubtract                     |                 11.29 |
|                59 | LocalVariablePrimitiveInplaceSubtract |                  9.33 |
|                62 | PrimitiveMultiply                     |                 13.76 |
|                66 | LocalVariablePrimitiveInplaceMultiply |                  9.64 |
|                69 | PrimitiveDivide                       |                 17.33 |
|                73 | LocalVariablePrimitiveInplaceDivide   |                 15.69 |

|   Opcode (UInt8) | Name                                  |   Estimated time (ns) |
|------------------|---------------------------------------|-----------------------|
|                2 | LocalVariableDeclareAssign            |                  0.00 |
|                8 | PushLocalVariable                     |                  0.63 |
|                9 | PopToLocalVariable                    |                  7.63 |
|               22 | ReturnValue                           |                 25.80 |
|               27 | LocalVariablePrefixInc                |                  1.56 |
|               28 | LocalVariablePrefixDec                |                  2.14 |
|               29 | LocalVariablePostfixInc               |                  1.82 |
|               30 | LocalVariablePostfixDec               |                  1.80 |
|               34 | PrimitiveEqual                        |                 14.69 |
|               36 | PrimitiveNotEqual                     |                 13.76 |
|               38 | PrimitiveLessThan                     |                 13.63 |
|               40 | PrimitiveLessThanOrEqual              |                 13.80 |
|               42 | PrimitiveGreaterThan                  |                 13.47 |
|               44 | PrimitiveGreaterThanOrEqual           |                 13.52 |
|               46 | PrimitiveNegate                       |                  5.21 |
|               48 | PrimitiveAdd                          |                 11.25 |
|               52 | LocalVariablePrimitiveInplaceAdd      |                  8.71 |
|               55 | PrimitiveSubtract                     |                 11.03 |
|               59 | LocalVariablePrimitiveInplaceSubtract |                  8.86 |
|               62 | PrimitiveMultiply                     |                 13.31 |
|               66 | LocalVariablePrimitiveInplaceMultiply |                  9.69 |
|               69 | PrimitiveDivide                       |                 13.52 |
|               73 | LocalVariablePrimitiveInplaceDivide   |                 10.97 |
