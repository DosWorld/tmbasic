#procedure
sub Main()
    print Foo(123)
end sub

#procedure
function Foo(x as Number) as Number
    dim map foo
        yield 1 to 1
        throw 999, "blah"
    end dim
end function

--output--
Error
999
blah