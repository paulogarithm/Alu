# At the end

Let's say you have this following file
```rs
// in foo.alu
let a = 3
print(a + 3)
```

To run the code:
```sh
./alu foo.alu
# <output>
```

To compile the code:
```sh
./alu foo.alu -c 
# Creates a foo.alc
```

To run a compiled code:
```sh
./alu foo.alc
# <output>
```