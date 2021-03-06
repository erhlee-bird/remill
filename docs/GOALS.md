# What problem Remill solves

Remill was designed with the following goals in mind.

- It should be easy to add new instruction implementations. Instruction semantics are implemented using C++. Instruction implementations should be thoroughly tested.

- Remill-produced bitcode should achieve the sometimes conflicting goals of maintaining the semantics of the translated machine code and enabling aggressive optimization of the produced bitcode.

- Decisions affecting the use of the produced bitcode should be deferred via intrinsics. Remill-produced bitcode should not commit a consumer of that bitcode to one use case.
