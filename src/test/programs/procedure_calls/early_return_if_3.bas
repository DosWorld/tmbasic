#procedure
sub Main()
    print Foo(123)
end sub

#procedure
function Foo(x as Number) as Number
    if x = 0 then
        throw 999, "blah"
    end if
    ' missing return here
end function

--output--
Compiler error
kControlReachesEndOfFunction
Foo
1:1
