#include "ipc.h"
#include "banking.h"

/** Transfer amount from src to dst.
 *
 * @param parent_data Any data structure implemented by students to perform I/O
 */
void transfer(void * parent_data, local_id src, local_id dst, balance_t amount) {

}

//------------------------------------------------------------------------------
// Functions below are implemented by lector, test implementations are
// provided to students for testing purposes
//------------------------------------------------------------------------------

/**
 * Returs the value of Lamport's clock.
 */
timestamp_t get_lamport_time() {
    return 0;
}

/** Returns physical time.
 *
 * Emulates physical clock (for each process).
 */
timestamp_t get_physical_time() {
    return 0;
}

/** Pretty print for BalanceHistories.
 *
 */
void print_history(const AllHistory * history);
