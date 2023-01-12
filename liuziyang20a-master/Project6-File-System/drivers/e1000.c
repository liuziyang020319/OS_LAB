#include <e1000.h>
#include <type.h>
#include <os/string.h>
#include <os/time.h>
#include <assert.h>
#include <pgtable.h>
#include <os/sched.h>


// E1000 Registers Base Pointer
volatile uint8_t *e1000;  // use virtual memory address

// E1000 Tx & Rx Descriptors
static struct e1000_tx_desc tx_desc_array[TXDESCS] __attribute__((aligned(16)));
static struct e1000_rx_desc rx_desc_array[RXDESCS] __attribute__((aligned(16)));

// E1000 Tx & Rx packet buffer
static char tx_pkt_buffer[TXDESCS][TX_PKT_SIZE];
static char rx_pkt_buffer[RXDESCS][RX_PKT_SIZE];

// Fixed Ethernet MAC Address of E1000
static const uint8_t enetaddr[6] = {0x00, 0x0a, 0x35, 0x00, 0x1e, 0x53};

/**
 * e1000_reset - Reset Tx and Rx Units; mask and clear all interrupts.
 **/
static void e1000_reset(void)
{
	/* Turn off the ethernet interface */
    e1000_write_reg(e1000, E1000_RCTL, 0);
    e1000_write_reg(e1000, E1000_TCTL, 0);

	/* Clear the transmit ring */
    e1000_write_reg(e1000, E1000_TDH, 0);
    e1000_write_reg(e1000, E1000_TDT, 0);

	/* Clear the receive ring */
    e1000_write_reg(e1000, E1000_RDH, 0);
    e1000_write_reg(e1000, E1000_RDT, 0);

	/**
     * Delay to allow any outstanding PCI transactions to complete before
	 * resetting the device
	 */
    latency(1);

	/* Clear interrupt mask to stop board from generating interrupts */
    e1000_write_reg(e1000, E1000_IMC, 0xffffffff);

    /* Clear any pending interrupt events. */
    while (0 != e1000_read_reg(e1000, E1000_ICR)) ;
}

/**
 * e1000_configure_tx - Configure 8254x Transmit Unit after Reset
 **/
static void e1000_configure_tx(void)
{
    /* TODO: [p5-task1] Initialize tx descriptors */
    for(int i=0; i<TXDESCS; i++){
        tx_desc_array[i].addr= (uint32_t)kva2pa((uintptr_t)(&(tx_pkt_buffer[i])));
        tx_desc_array[i].cmd = 0;
        tx_desc_array[i].status = 0;
        // tx_desc_array[i].status |= (1<<0);
        tx_desc_array[i].cmd|=(1<<0);//EOP
        tx_desc_array[i].cmd|=(1<<3);//RS
    }

    /* TODO: [p5-task1] Set up the Tx descriptor base address and length */
    
    uint32_t LOW  = (uint32_t)(((uint64_t)kva2pa(tx_desc_array)) & 0xffffffff);
    uint32_t HIGH = (uint32_t)(((uint64_t)kva2pa(tx_desc_array)) >> 32); 
    uint32_t SIZE = 16*TXDESCS;
    
    e1000_write_reg(e1000,E1000_TDBAL,LOW);
    e1000_write_reg(e1000,E1000_TDBAH,HIGH);
    e1000_write_reg(e1000,E1000_TDLEN,SIZE);

	/* TODO: [p5-task1] Set up the HW Tx Head and Tail descriptor pointers */

    e1000_write_reg(e1000,E1000_TDH,0);
    e1000_write_reg(e1000,E1000_TDT,0);

    /* TODO: [p5-task1] Program the Transmit Control Register */

    uint32_t TCTL_write = 0;
    //EN
    TCTL_write |= E1000_TCTL_EN;
    TCTL_write |= E1000_TCTL_PSP;
    TCTL_write |= 0x100;
    TCTL_write |= 0x40000;

    e1000_write_reg(e1000,E1000_TCTL,TCTL_write);
    local_flush_dcache();
}

/**
 * e1000_configure_rx - Configure 8254x Receive Unit after Reset
 **/
static void e1000_configure_rx(void)
{
    /* TODO: [p5-task2] Set e1000 MAC Address to RAR[0] */
    //RAL0
    uint32_t RAL0_v = (enetaddr[3]<<24) |
                      (enetaddr[2]<<16) |
                      (enetaddr[1]<< 8) |
                      (enetaddr[0])
                      ;
    
    uint32_t RAH0_v = E1000_RAH_AV      |
                      (enetaddr[5]<< 8) |
                      (enetaddr[4])
                      ;

    e1000_write_reg_array(e1000,E1000_RA,0,RAL0_v);
    e1000_write_reg_array(e1000,E1000_RA,1,RAH0_v);

    /* TODO: [p5-task2] Initialize rx descriptors */

    for(int i=0; i<RXDESCS; i++){
        rx_desc_array[i].addr = (uint32_t)kva2pa((uintptr_t)(&(rx_pkt_buffer[i])));
        rx_desc_array[i].status = 0;
        // rx_desc_array[i].status |= (0<<1);//EOP
    }

    /* TODO: [p5-task2] Set up the Rx descriptor base address and length */
    
    uint32_t LOW  = (uint32_t)(((uint64_t)kva2pa(rx_desc_array)) & 0xffffffff);
    uint32_t HIGH = (uint32_t)(((uint64_t)kva2pa(rx_desc_array)) >> 32); 
    uint32_t SIZE = 16*RXDESCS;
    
    e1000_write_reg(e1000,E1000_RDBAL,LOW);
    e1000_write_reg(e1000,E1000_RDBAH,HIGH);
    e1000_write_reg(e1000,E1000_RDLEN,SIZE);

    /* TODO: [p5-task2] Set up the HW Rx Head and Tail descriptor pointers */

    e1000_write_reg(e1000,E1000_RDH,0);
    e1000_write_reg(e1000,E1000_RDT,RXDESCS -1);

    /* TODO: [p5-task2] Program the Receive Control Register */

    uint32_t RCTL_write = E1000_RCTL_EN    | 
                          E1000_RCTL_BAM   |
                          ~E1000_RCTL_BSEX |
                          E1000_RCTL_SZ_2048
                          ;

    e1000_write_reg(e1000,E1000_RCTL,RCTL_write);

    /* TODO: [p5-task4] Enable RXDMT0 Interrupt */

    local_flush_dcache();
}

/**
 * e1000_init - Initialize e1000 device and descriptors
 **/
void e1000_init(void)
{
    /* Reset E1000 Tx & Rx Units; mask & clear all interrupts */
    e1000_reset();

    /* Configure E1000 Tx Unit */
    e1000_configure_tx();

    /* Configure E1000 Rx Unit */
    e1000_configure_rx();
}

/**
 * e1000_transmit - Transmit packet through e1000 net device
 * @param txpacket - The buffer address of packet to be transmitted
 * @param length - Length of this packet
 * @return - Number of bytes that are transmitted successfully
 **/

void e1000_block_send(){
    current_running = get_current_cpu_id()? &current_running_1 : &current_running_0;
    (*current_running)->status = TASK_BLOCKED;
    list_del(&((*current_running)->list));
    list_add(&((*current_running)->list), &send_queue);
    do_scheduler(); 
}

void e1000_check_send(){
    current_running = get_current_cpu_id()? &current_running_1 : &current_running_0;
    local_flush_dcache();
    if(send_queue.prev == &send_queue) return;
    list_node_t *now = &send_queue;
    list_node_t *nxt = now->prev;

    int tail = e1000_read_reg(e1000,E1000_TDT);
    if(tx_desc_array[tail].status & E1000_TXD_STAT_DD){
        list_del(nxt);
        list_add(nxt,&ready_queue);
        pcb_t *pcb_now = LIST_to_PCB(nxt);
        pcb_now->status = TASK_READY;        
    }
}
int cnt = 0; 
int e1000_transmit(void *txpacket, int length)
{
    /* TODO: [p5-task1] Transmit one packet from txpacket */
    local_flush_dcache();
    int tail = e1000_read_reg(e1000,E1000_TDT);
    // int head = e1000_read_reg(e1000,E1000_TDH);
    // int TCTL = e1000_read_reg(e1000,E1000_TCTL);

    if(cnt > TXDESCS && (tx_desc_array[tail].status % 2 == 0)){
        e1000_block_send();
    }

    cnt ++;

    tx_desc_array[tail].length = length;

    // printl("L = %d\n",length);

    tx_desc_array[tail].status &= ~(E1000_TXD_STAT_DD);

    memcpy(tx_pkt_buffer[tail],txpacket,length);
    
    tail = (tail + 1) % TXDESCS;
    e1000_write_reg(e1000,E1000_TDT,tail);

    local_flush_dcache();
    return 0;
}



/**
 * e1000_poll - Receive packet through e1000 net device
 * @param rxbuffer - The address of buffer to store received packet
 * @return - Length of received packet
 **/

int32_t nxt = 0;

void e1000_block_recv(){
    current_running = get_current_cpu_id()? &current_running_1 : &current_running_0;
    (*current_running)->status = TASK_BLOCKED;
    list_del(&((*current_running)->list));
    list_add(&((*current_running)->list), &recv_queue);
    do_scheduler(); 
}

void e1000_check_recv(){
    current_running = get_current_cpu_id()? &current_running_1 : &current_running_0;
    local_flush_dcache();
    if(recv_queue.prev == &recv_queue) return;
    list_node_t *now = &recv_queue;
    now = now->prev;

    if(rx_desc_array[nxt].status & E1000_RXD_STAT_DD){
        list_del(now);
        list_add(now,&ready_queue);
        pcb_t *pcb_now = LIST_to_PCB(now);
        pcb_now->status = TASK_READY;        
    }
}

int e1000_poll(void *rxbuffer)
{
    /* TODO: [p5-task2] Receive one packet and put it into rxbuffer */
    
    local_flush_dcache();

    int tail = e1000_read_reg(e1000,E1000_RDT);
    int head = e1000_read_reg(e1000,E1000_RDH);

    // while(!(rx_desc_array[nxt].status & E1000_RXD_STAT_DD)){
    //     local_flush_dcache();
    // };

   

    if((rx_desc_array[nxt].status % 2 == 0)){
        e1000_block_recv();
    }
    
    int32_t len = rx_desc_array[nxt].length;
    memcpy(rxbuffer,rx_pkt_buffer[nxt],len);

    rx_desc_array[nxt].status = 0;
    
    tail = (tail + 1) % RXDESCS;
    nxt = (nxt + 1) % RXDESCS;

    e1000_write_reg(e1000,E1000_RDT,tail);

    local_flush_dcache();
    return len;
}