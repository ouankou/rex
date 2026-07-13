void rex_openacc_wait_forms(int queue) {
#pragma acc wait
#pragma acc wait(queue)
#pragma acc wait(devnum : queue)
#pragma acc wait(queues : queue)
#pragma acc wait(devnum:queue:queues : queue, queue + 1)
}
