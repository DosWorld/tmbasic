#procedure
sub Main()
    print Foo(123)
end sub

#procedure
function Foo(x as Number) as Number
    do
        exit do
        return 1
    loop while x = 5
end function

--output--
Compiler error
kControlReachesEndOfFunction
Foo
1:1
