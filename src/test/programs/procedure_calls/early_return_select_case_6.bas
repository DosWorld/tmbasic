#procedure
sub Main()
    print Foo(123)
end sub

#procedure
function Foo(x as Number) as Number
    select case x
        case 1
            throw 999, "blah"
        case 2
            throw 999, "blah"
    end select
end function

--output--
Compiler error
kControlReachesEndOfFunction
Foo
1:1
