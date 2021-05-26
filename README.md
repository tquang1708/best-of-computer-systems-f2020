# best-of-computer-systems-f2020
Some labs that I'm kinda proud of that came from the Computer Systems class of Fall 2020, being the Attack Lab, the Shell Lab and the Malloc Lab

## Attack Lab

> So what exactly is the attack lab? I was going to write a whole paragraph explaining what it is, what are its purposes and what we will learn, but the given handout from CMU’s course is way more detailed and well-written than whatever I was trying to write.
> 
> In essence, we are given a program that is intentionally exploitable, and our job is to, well, exploit it, utilizing Code Injection and Return-Oriented Programming attacks.
> 
> Of course, as mentioned, this program is intentionally exploitable. However, there do exist systems and programs that are unintentionally exploitable. Through this lab, aside from having a better understanding of the steps a program take while running, we should be able to also be aware of these exploits in an effort to not let them show up in our own softwares.

## Shell Lab

> ...the shell lab, in which we are tasked to write "a simple Unix shell that supports job control," in order to be "more familiar with the concepts of process control and signalling."

## Malloc Lab

> ...the Malloc lab, which, according to the handout, asks us to write "a dynamic storage allocator for C programs, i.e., your own version of the malloc, free and realloc routines. You are encouraged to explore the design space creatively and implement an allocator that is correct, efficient and fast."

> I originally was planning to go for a segregated list approach. Aside from it being the closest thing to the actual C Malloc implementation, it also provides really good efficiency guarantees in comparison to other methods, like O(log(n)) lookup time for malloc (n being the total number of free blocks). However, due to time constraint, I was only able to implement an explicit list version. It is to be noted though that theoretically speaking, modifying this to become a segregated list is rather simple.

> My implementation sufficiently passed all traces except for the realloc traces (since I did not have a proper realloc implementation), plus the random traces (which caused my computer to run out of memory since it had like over 2000 malloc calls). In a sense, it performs well enough that it should be usable in 90% of common cases, though of course that means it's not good enough for any production-level softwares.

> Notes on how to overcome the remaining traces, plus theories on how to turn this into a segregated list is included in the appendix.

> As for the what exactly is an explicit list implementation, in essence, instead of "implicitly" keeping a list of the free blocks, we explicitly keep track of the free blocks, and only the free blocks. Therefore, whenever we need to do a malloc, instead of searching through the entire list of all blocks to find a free block big enough for the required memory, we just search through the explicit list instead.

> In order to do this, we implement the explicit list as a doubly linked list - doubly to make it easier to do insertion/deletion in the middle of the list. This means every free block would have a next pointer and a prev pointer, pointing to the next and previous free blocks in the list respectively. Since the program does not care about data stored in a free block, we could simply use that space to store our pointers. Better visualization and explanation could be seen on the malloc lab slide.

### Random traces and Realloc traces

> The Random traces are a collection of randomly generated malloc/free instructions, aimed to test the robustness of the implementation. The main problem I ran into with that trace concerning this implementation is the fact that it failed when memory runs out, which is expected.
> 
> A solution would be to redo it with a better design that make for less fragmentation. There is none that I can come up with on the spot, but there definitely exists better implementation out there.
> 
> Another idea would be to test this on the Jupyter server instead of on my own machine. I basically downloaded all the traces to my machine then did everything locally, so maybe this wouldn't have even been a problem if I submitted the work online.

> The Realloc traces have additional realloc instructions aside from the normal malloc/free instructions. The naive approach would be to simply free the target block then malloc a new block with a new size, though as noted in the realloc readme, this is not ideal for efficiency. 
> 
> Concerning this implementation, a solution would be to see whether there are free blocks immediately after the block that is being realloc'd that could be combined with the current malloc to extend the size. Reducing the size, on the other hand, simply involves cutting up the old malloc block then coalesce whatever remains.

### Segregated List

> The segregated list involve using multiple free lists corresponding to different size class instead of just a single free list like in our current implementation. We could do this by either just adding extra static root pointers. However, my personal preferred method would be to have the root lives contiguously on the heap. This way, we could access the root list addresses using pointer manipulation similar to accessing an array (this is in fact just a contiguous array of pointers pointing at the root of the free lists). Simpler accessing means cleaner code: we could use a simple hash function to get the corresponding free list array indices for every block size, similar to a hash table.
