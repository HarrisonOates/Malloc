# Report

<!-- Describe implementation of explicit free list, fence posts, constant time coaleescing -->

I present an explicit, multiple free list malloc implementation with fence posts and constant time coalescing, alongside a rough implementation of the garbage collector. I discuss both of these below.

Memory blocks returned by `malloc` are sandwiched between the `Header` and `Footer` data structures. The former contains size and free list pointer metadata, and the latter only containing size so as to enable constant time coalescing.
The free lists themselves are held in a 59-element list of type `Header*[]`, where the appropriate list can be accessed in constant time by a call to `getIndex`.
When memory is requested from the OS via `getMemory()`, fenceposts, consisting of a `Footer` on the left and `Header` on the right, are inserted. This approach was chosen to avoid implementing an additional data structure. As further safeguards against freeing across chunks, we use `leftBound` and `rightBound` to highlight the edges of the heap. This is a reasonable approach because we cannot manage memory outside of the heap, so even if there is non-addressable memory within this range we can still safeguard against segfaults.

<!-- Optimizations attempted in the implementation of malloc -->
I successfully reduced metadata by using the least significant bit of size to indicate whether a block was allocated, alongside placing `next` and `prev` at the end of `Header` so I can allocate the saved space to the user. To achieve time coalescing, the size metadata was stored in `Footer` for each block, allowing the centre block to look to the left by `sizeof(Footer)` to get the jump distance. Adapting the single free list design to multiple free lists necessitated passing the list index to my list operation functions (`addToList, removeFromList, traverseListForSize`) and adapting `split_block` to add the 'offcut' to an appropriate list. 

<!-- Two implementation challenges in implementation of malloc -->
My first implementation challenge was in deciding the appropriateness of pointers between `void *, Header *` and `size_t`. Multiple times I ran `test.py` and got segfaulted with one choice, only to change it and pass the test I failed previously. This was solved as I understood the problem space more and saw the same lines of code being repeated throughout the assignment.
My second implementation challenge was in adapting my implementation to multiple free lists. This necessitated significantly more bookkeeping, incurring a slight performance penalty in the benchmark (on the order of 0.08 seconds on my Ryzen 5 3600 / 3200MHz system).






<!-- Two key observations from testing and benchmarking malloc implementation. What broke? -->
My first observation regarding testing relates to the process of writing the code. Initially, I had written significant chunks of the code at once, compiling regularly to fix errors. I would run `test.py` to work out what needed fixing, and changed stuff that appeared to be the problem until the tests passed.
This worked alright until I tried to run `bench.py` , where it proceeded to hang my machine, crash my desktop environment for several minutes, and cause VSCode to shut.
After much frustration, I learned how to use GDB instead of just relying on the IDE and started the implementation again from the ground up, running tests and debugging far more often.
This was much more successful, passing the test suite and `bench.py` with an average time of 0.689s (Ryzen 5 3600, 3200MHz).
My second observation regarding the benchmark was how useful it was in debugging memory errors, particularly regarding out-of-bounds accesses.
Many issues only popped up for me only after a few hundred iterations, which I only became aware of because of the benchmark. Using GDB's `bt` and `p` commands on the benchmark after an exception was thrown meant I could pinpoint my issues far more effectively.