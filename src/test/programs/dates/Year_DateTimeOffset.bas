sub Main()
    dim x = DateTimeOffsetFromParts(2021, 3, 12, 4, 30, 59, 488, Hours(-4))
    print Year(x)
end sub

--output--
2021
