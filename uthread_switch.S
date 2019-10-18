	.text

/* Switch from current_thread to next_thread. Make next_thread
 * the current_thread, and set next_thread to 0.
 * Use eax as a temporary register; it is caller saved.
 */
	.globl thread_switch
thread_switch:
	pushal					/*Save general purpose registers*/
	
	movl current_thread, %eax
	movl %esp, (%eax) 			/*Save the stack pointer for current_thread*/

	movl next_thread, %eax
	movl (%eax), %esp 			/*Restore next_thread's stack pointer*/

	movl %eax, current_thread		/*set current_thread = next_thread*/
	movl $0, next_thread			/*set next_thread = 0*/

	popal					/*Restore general purpose registers*/

	ret					/* pop return address from stack */