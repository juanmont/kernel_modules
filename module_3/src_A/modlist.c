#include <linux/module.h> /* Requerido por todos los módulos */
#include <linux/kernel.h> /* Definición de KERN_INFO */
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/list.h>
#include <asm-generic/uaccess.h>
#include <linux/spinlock.h>

MODULE_LICENSE("GPL"); /* Licencia del módulo */

//no es buena idea
#define BUFFER_LENGTH       PAGE_SIZE/8



struct list_head mylist; /* Lista enlazada */


DEFINE_SPINLOCK(sp); //Spin-lock
 

/* Nodos de la lista */
typedef struct list_item_t {
	int data;
	struct list_head links;
};


static struct proc_dir_entry *proc_entry;
static char *info;  // buffer para almacenar la entrada de texto

static ssize_t modlist_read(struct file *filp, char __user *buf, size_t len, loff_t *off){

	struct list_item_t* item=NULL;
	struct list_head* cur_node=NULL;
	char kbuf[BUFFER_LENGTH];
	char* dest=kbuf;	
	int i = 0;
	static int finished = 0;

	if (finished) {
			finished = 0;
			return 0;
	}
	
	finished = 1;
	
	spin_lock(&sp);
	//comprobar desbordamiento de buffer
	list_for_each(cur_node, &mylist) { /* item points to the structure wherein the links are embedded */
			 
		item = list_entry(cur_node,struct list_item_t, links);
		printk(KERN_INFO "%i\n",item->data);	
		dest+=sprintf(dest,"%i\n",item->data);
			
	}
	spin_unlock(&sp);



	i=dest-kbuf;
		
	if (copy_to_user(buf, kbuf,i))
		return -EFAULT;
		
	return i;	 
}

static ssize_t modlist_write(struct file *filp, const char __user *buf, size_t len, loff_t *off){
	int num;
	
	struct list_item_t* tmp=NULL;
	struct list_head *pos, *q;
	
	if (copy_from_user(&info[0], buf, len))
		return -EFAULT;

	if (sscanf(info, "add %i", &num) == 1){
		struct list_item_t *node = (struct list_item_t *)vmalloc(sizeof(struct list_item_t));
		
		
			
			node->data = num;
			INIT_LIST_HEAD(&node->links);
			spin_lock(&sp);
				list_add_tail(&node->links, &mylist);
			spin_unlock(&sp);

		
	}
	
	
	
	else if (sscanf(info, "remove %i", &num) == 1){
		
		spin_lock(&sp);
		list_for_each_safe(pos, q, &mylist){
		 tmp= list_entry(pos, struct list_item_t, links);
			if (num == tmp->data){
				list_del(pos);
				vfree(tmp);
			}	 
		}
		spin_unlock(&sp);	
	}
	else if (strncmp(info,"cleanup", 7) == 0){
		spin_lock(&sp);
		list_for_each_safe(pos, q, &mylist){
			tmp= list_entry(pos, struct list_item_t, links);
			list_del(pos);
			vfree(tmp);
		}
		spin_unlock(&sp);
	}
	
	return len;
}



static const struct file_operations proc_entry_fops = {
    .read = modlist_read,
    .write = modlist_write,    
};




/* Función que se invoca cuando se carga el módulo en el kernel */
int modulo_lin_init(void)
{
	INIT_LIST_HEAD(&mylist); //inicializamos la lista
	int ret = 0;
	info = (char *)vmalloc( BUFFER_LENGTH ); //reservamos memoria para el buffer de entrada

	if (!info) 
		ret = -ENOMEM;
	else {

		memset( info, 0, BUFFER_LENGTH );
		proc_entry = proc_create( "modlist", 0666, NULL, &proc_entry_fops);
    
		if (proc_entry == NULL) {
			ret = -ENOMEM;
			vfree(info);
			printk(KERN_INFO "no se puede insertar el modulo \n");
		} 
		else 
      printk(KERN_INFO "modlist: Module loaded\n");
  }

  return ret;
}
/* Función que se invoca cuando se descarga el módulo del kernel */
void modulo_lin_clean(void)
{
	struct list_item_t* tmp=NULL;
	struct list_head *pos, *q;
	
	//write_lock_irq(&rwl);
	list_for_each_safe(pos, q, &mylist){
		 tmp= list_entry(pos, struct list_item_t, links);
		 list_del(pos);
		 vfree(tmp);
	}
	//write_unlock_irq(&rwl);

	
	
 remove_proc_entry("modlist", NULL);
  vfree(info);
  printk(KERN_INFO "modlist: Module unloaded.\n");
}

/* Declaración de funciones init y cleanup */
module_init(modulo_lin_init);
module_exit(modulo_lin_clean);
