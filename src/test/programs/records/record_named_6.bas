type Foo
    z as Number
end type
sub Main()
    dim bar = { a: 5 } as Foo
end sub
--output--
Compiler error
kInvalidTypeConversion
Main
2:15
