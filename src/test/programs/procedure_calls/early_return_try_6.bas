#procedure
sub Main()
    print Foo(123)
end sub

#procedure
function Foo(x as Number) as Number
    try
    catch
        throw 999, "blah"
    end try
end function

--output--
Compiler error
kControlReachesEndOfFunction
Foo
1:1
