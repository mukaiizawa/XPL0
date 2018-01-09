XPL0 -- extended Wirth's pl/0 compiler.

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
       1: const a = 3;
       2: var x, y;
       3: 
       4: begin
       5:   x := 1 + 3 * 5;
       6:   y := a * x + 3;
       7: end.

    *** table ***
    name	object_type	level	address	value
    a	constant	-	-	3
    x	variable	0	3	-
    y	variable	0	4	-

    *** instruction ***
       0: JMP 0,1
       1: INT 0,5
       2: LIT 0,1
       3: LIT 0,3
       4: LIT 0,5
       5: OPR 0,4
       6: OPR 0,2
       7: STO 0,3
       8: LIT 0,3
       9: LOD 0,3
      10: OPR 0,4
      11: LIT 0,3
      12: OPR 0,2
      13: STO 0,4
      14: OPR 0,0

    *** start xpl0 ***
    p   a,l		b	t	s
    JMP 1,0		1	0	0 > < 0 0 0 0 
    INT 5,0		1	0	0 > < 0 0 0 0 
    LIT 1,0		1	5	0 > 0 0 0 0 0 < 0 0 0 0 
    LIT 3,0		1	6	0 > 0 0 0 0 0 1 < 0 0 0 0 
    LIT 5,0		1	7	0 > 0 0 0 0 0 1 3 < 0 0 0 0 
    OPR 4,0		1	8	0 > 0 0 0 0 0 1 3 5 < 0 0 0 0 
    OPR 2,0		1	7	0 > 0 0 0 0 0 1 15 < 5 0 0 0 
    STO 3,0		1	6	0 > 0 0 0 0 0 16 < 15 5 0 0 
    assign 16
    LIT 3,0		1	5	0 > 0 0 0 16 0 < 16 15 5 0 
    LOD 3,0		1	6	0 > 0 0 0 16 0 3 < 15 5 0 0 
    OPR 4,0		1	7	0 > 0 0 0 16 0 3 16 < 5 0 0 0 
    LIT 3,0		1	6	0 > 0 0 0 16 0 48 < 16 5 0 0 
    OPR 2,0		1	7	0 > 0 0 0 16 0 48 3 < 5 0 0 0 
    STO 4,0		1	6	0 > 0 0 0 16 0 51 < 3 5 0 0 
    assign 51
    OPR 0,0		1	5	0 > 0 0 0 16 51 < 51 3 5 0 
