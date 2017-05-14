SOPE 2016-2017 - T1G08 - second project

André Filipe de Soveral Torres Lopes dos Santos - up200505634
Ilona Generalova - up201400035
Tiago Lascasas dos Santos - up201503616

We have implemented two synchronization mechanisms: the first
was in the mainThreadFunction() of sauna.c, in which we had to
wait until a place in the sauna was vacant, that is, until the
variable freeSlots decreases. We decided, then, to use a condition
variable that waits while there aren't any free seats until one of
the "occupant" threads decreases that variable. Since there are
multiple threads and since the freeSlots variable is global, we had
to ensure that this decreasing operation had to be done atomically.

The second synchronization mechanism was done in both sauna.c and
generator.c. In both programs, writing to the log is an operation
that can be done concurrently by multiple threads, and since the
write() function, when it comes to files, is not assured to be done
atomically, we had to use a mutex on the writeRequestToLog function,
which is the only function in both programs that writes to their logs.

It is also worth noticing that the writings to the FIFOs in our programs
is always done atomically by default. The writing operation on FIFOs is
always atomic when the number of bytes to write does not exceed PIPE_BUF
bytes, a value that is never shorter than 512 bytes. The only thing we
write to the FIFOs are structs request_t (always one and only one per call),
and since the size of request_t is smaller than 512 bytes, we can assure
that all writings are atomic.
