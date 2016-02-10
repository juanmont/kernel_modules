
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <asm-generic/uaccess.h>
#include <linux/semaphore.h>
#include <linux/fs.h>

#include "cbuffer.h"

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("fifoproc Kernel Module - FDI-UCM");
MODULE_AUTHOR("");


/* fifoproc.h */
int init_module(void);
void cleanup_module(void);
static int fifo_open(struct inode *, struct file *);
static int fifo_release(struct inode *, struct file *);
static ssize_t fifo_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t fifo_write(struct file *, const char __user *, size_t, loff_t *);

#define BUFFER_LENGTH       PAGE_SIZE
#define MAX_CBUFFER_LEN 100
#define DEVICE_NAME "fifomod"


cbuffer_t* cbuffer; /* Buffer circular */
int prod_count = 0; /* Número de procesos que abrieron la entrada
/proc para escritura (productores) */
int cons_count = 0; /* Número de procesos que abrieron la entrada
/proc para lectura (consumidores) */
struct semaphore mtx; /* para garantizar Exclusión Mutua */
struct semaphore sem_prod; /* cola de espera para productor(es) */
struct semaphore sem_cons; /* cola de espera para consumidor(es) */
int nr_prod_waiting=0; /* Número de procesos productores esperando */
int nr_cons_waiting=0; /* Número de procesos consumidores esperando */
static int Major;

//----------------------


static struct file_operations fops = {
	.read = fifo_read,
	.write = fifo_write,
	.open 	 = fifo_open,
	.release = fifo_release
};



int init_module(void)
{
	
	Major = register_chrdev(0, DEVICE_NAME, &fops);
	
	 if (Major < 0){	 
		printk(KERN_ALERT "Registering char device failed with %d\n", Major);         
		return Major;        
	}
 
	
	//Inicializamos semaforos de mutex y de colas de espera
	sema_init(&mtx,1);
	sema_init(&sem_cons,0);
	sema_init(&sem_prod,0);
	
	
	//Inicializamos cbuffer
	cbuffer = create_cbuffer_t (MAX_CBUFFER_LEN);
	

	 printk(KERN_INFO "I was assigned major number %d. To talk to\n", Major);        
	 printk(KERN_INFO "the driver, create a dev file with\n");
     printk(KERN_INFO "'mknod /dev/%s c %d 0'.\n", DEVICE_NAME, Major);        
     printk(KERN_INFO "Try various minor numbers. Try to cat and echo to\n");       
     printk(KERN_INFO "the device file.\n");        
     printk(KERN_INFO "Remove the device file and module when done.\n");
   	
  	return 0;
}


void cleanup_module( void )
{
	destroy_cbuffer_t(cbuffer);      
	unregister_chrdev(Major, DEVICE_NAME);      
	
	 
	
		
  	printk(KERN_INFO "Fifoproc: Module unloaded.\n");
}



/* Se invoca al hacer open() de entrada /proc */
static int fifo_open(struct inode *inode, struct file *file)
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
static int fifo_release(struct inode *inode, struct file *file){
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
		clear_cbuffer_t(cbuffer);
	up(&mtx);
	return 0;
}




/* Se invoca al hacer read() de entrada /proc */
static ssize_t fifo_read(struct file *file, char __user *buf, size_t len, loff_t *off){
	char kbuffer[MAX_CBUFFER_LEN];
	if (len> MAX_CBUFFER_LEN) {
		return -EFAULT;
	}
	if (down_interruptible(&mtx)){
		return -EINTR;
	}
	while (size_cbuffer_t(cbuffer)<len && prod_count>0){
		nr_cons_waiting +=1;
		up(&mtx);
		if (down_interruptible(&sem_cons)){
			return -EINTR;	
		}
	}
	
	if(prod_count == 0 && size_cbuffer_t(cbuffer) == 0){
		up(&mtx);
		return 0;
	}
	
	remove_items_cbuffer_t(cbuffer,kbuffer,len);
	
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
static ssize_t fifo_write(struct file *file, const char __user *buf, size_t len, loff_t *off){
	
	char kbuffer[MAX_CBUFFER_LEN];
	
	if(len > MAX_CBUFFER_LEN ){ 
		return -EFAULT;
	}
	if (copy_from_user(kbuffer,buf,len)) {
		return -EFAULT;
	}
	if (down_interruptible(&mtx)){
		return -EINTR;
	}
	/* Esperar hasta que haya hueco para insertar (debe haber consumidores) */
	while (nr_gaps_cbuffer_t(cbuffer)<len && cons_count>0){
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
	
	insert_items_cbuffer_t(cbuffer,kbuffer,len);
	/* Despertar a posible consumidor bloqueado */
	if(nr_cons_waiting > 0){
		up(&sem_cons);
		nr_cons_waiting -=1;
	}
	up(&mtx);
	return len;

}

