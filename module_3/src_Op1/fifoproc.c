
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <asm-generic/uaccess.h>
#include <linux/ftrace.h>
#include <linux/semaphore.h>
#include <linux/kfifo.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("fifoproc Kernel Module - FDI-UCM");
MODULE_AUTHOR("");
#define BUFFER_LENGTH       PAGE_SIZE
#define MAX_FIFO_LEN 100

struct kfifo fifo; /* FIFO */
int ret;
int prod_count = 0; /* Número de procesos que abrieron la entrada
/proc para escritura (productores) */
int cons_count = 0; /* Número de procesos que abrieron la entrada
/proc para lectura (consumidores) */
struct semaphore mtx; /* para garantizar Exclusión Mutua */
struct semaphore sem_prod; /* cola de espera para productor(es) */
struct semaphore sem_cons; /* cola de espera para consumidor(es) */
int nr_prod_waiting=0; /* Número de procesos productores esperando */
int nr_cons_waiting=0; /* Número de procesos consumidores esperando */

static struct proc_dir_entry *proc_entry;

/* Funciones de inicialización y descarga del módulo */
int init_module(void);
void cleanup_module(void);

/* Se invoca al hacer open() de entrada /proc */
static int fifoproc_open(struct inode *inode, struct file *file)
{
	if (down_interruptible(&mtx)){
		return -EINTR;
	}
	if(file->f_mode & FMODE_READ){
		cons_count +=1;
		//cond_signal(prod)
		up(&sem_prod);
		nr_prod_waiting -=1;
		while(prod_count == 0){
			//cond_wait(cons,mtx);
			nr_cons_waiting +=1;
			up(&mtx);
			if (down_interruptible(&sem_cons)){
				return -EINTR;
			}
		}
		
	}else{
		prod_count +=1;
		//cond_signal(cons)
		up(&sem_cons);
		nr_cons_waiting -=1;
		while(cons_count == 0){
			//cond_wait(cons,mtx);
			nr_prod_waiting +=1;
			up(&mtx);
			if (down_interruptible(&sem_prod)){
				return -EINTR;
			}	
		}
		
	}
	up(&mtx);
	return 0;
}
/* Se invoca al hacer close() de entrada /proc */
static int fifoproc_release(struct inode *inode, struct file *file){
	if (down_interruptible(&mtx)){
		return -EINTR;
	}
	if(file->f_mode & FMODE_READ){
		cons_count -=1;
		//cond_signal(prod)
		if(nr_prod_waiting > 0){
			up(&sem_prod);
			nr_prod_waiting -=1;
		}
		
	}else{
		prod_count -=1;
		//cond_signal(cons)
		if(nr_cons_waiting > 0){
			up(&sem_cons);
			nr_cons_waiting -=1;
		}
		
	}
	if(prod_count == 0 && cons_count == 0)
		kfifo_reset(&fifo);
	up(&mtx);
	return 0;
}




/* Se invoca al hacer read() de entrada /proc */
static ssize_t fifoproc_read(struct file *file, char __user *buf, size_t len, loff_t *off){
	char kbuffer[len+1];
	if (len> MAX_FIFO_LEN) {
		return -EFAULT;
	}
	if (down_interruptible(&mtx)){
		return -EINTR;
	}
	while (kfifo_len(&fifo)<len && prod_count>0){
		nr_cons_waiting +=1;
		up(&mtx);
		if (down_interruptible(&sem_cons)){
			return -EINTR;	
		}
	}
	
	if(prod_count == 0 && kfifo_len(&fifo) == 0){
		up(&mtx);
		return 0;
	}
	
	kfifo_out(&fifo,kbuffer,len);
	
	
	if(nr_prod_waiting > 0){
		up(&sem_prod);
		nr_prod_waiting -=1;
	}
	up(&mtx);
	
	if (copy_to_user(buf,kbuffer,len)) {
		return -EFAULT;
	}
	return len;	
}


/* Se invoca al hacer write() de entrada /proc */
static ssize_t fifoproc_write(struct file *file, const char __user *buf, size_t len, loff_t *off){
	
	char kbuffer[len+1];
	
	if(len > MAX_FIFO_LEN ){ 
		return -EFAULT;
	}
	if (copy_from_user(kbuffer,buf,len)) {
		return -EFAULT;
	}
	if (down_interruptible(&mtx)){
		return -EINTR;
	}
	/* Esperar hasta que haya hueco para insertar (debe haber consumidores) */
	while (kfifo_avail(&fifo)<len && cons_count>0){
		nr_prod_waiting +=1;
		up(&mtx);
		if (down_interruptible(&sem_prod)){
			return -EINTR;
		}	
	}
	
	/* Detectar fin de comunicación por error (consumidor cierra FIFO antes) */
	if (cons_count==0) {
		up(&mtx); 
		return -EPIPE;
	}
	
	kfifo_in(&fifo,kbuffer,len);
	/* Despertar a posible consumidor bloqueado */
	if(nr_cons_waiting > 0){
		up(&sem_cons);
		nr_cons_waiting -=1;
	}
	up(&mtx);
	return len;

}


static const struct file_operations proc_entry_fops = {
	.read = fifoproc_read,
	.write = fifoproc_write,
	.open 	 = fifoproc_open,
	.release = fifoproc_release, 
};



int init_fifoproc_module( void )
{
	//Inicializamos semaforos de mutex y de colas de espera
	sema_init(&mtx,1);
	sema_init(&sem_cons,0);
	sema_init(&sem_prod,0);
	
	//Inicializamos fifo
	ret = kfifo_alloc(&fifo, BUFFER_LENGTH, GFP_KERNEL);
	if(ret)
		return ret;

  
   	proc_entry = proc_create( "fifoproc", 0666, NULL, &proc_entry_fops);
   	if (proc_entry == NULL) {
      		printk(KERN_INFO "Fifoproc: Can't create /proc entry\n");
      		return -ENOMEM;
    	} else {
      		printk(KERN_INFO "Fifoproc: Module loaded\n");
    	} 
  	return 0;
}


void exit_fifoproc_module( void )
{
  	remove_proc_entry("Fifoproc", NULL);
	kfifo_free(&fifo);
  	printk(KERN_INFO "Fifoproc: Module unloaded.\n");
}

module_init( init_fifoproc_module );
module_exit( exit_fifoproc_module );
