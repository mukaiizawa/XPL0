XPL0 -- extended pl/0 compiler

# usage
Make executable file.
    $make os={windows | linux} [debug=off]

Execute program.
    $cat test.xpl0
    const m = 7, n = 85;
    var x;
    begin
      x := m + n;
    end.

    $xpl0 < test.xpl0
    *** source ***
       1: const m = 7, n = 85;
       2: var x;
       3: 
       4: begin
       5:   x := m + n;
       6: end.

    *** table ***
    name	object_type	level	address	value
    m	constant	0	1	7
    n	constant	0	0	85
    x	variable	0	3	0

    *** instruction ***
       0: JMP 0,1
       1: INT 0,4
       2: LIT 0,7
       3: LIT 0,85
       4: OPR 0,2
       5: STO 0,3
       6: OPR 0,0

    *** start xpl0 ***
    assign 92 to x
