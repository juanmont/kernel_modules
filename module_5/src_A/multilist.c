#include <linux/module.h> /* Requerido por todos los módulos */
#include <linux/kernel.h> /* Definición de KERN_INFO */
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/list.h>
#include <asm-generic/uaccess.h>
#include <linux/spinlock.h>

MODULE_LICENSE("GPL"); /* Licencia del módulo */


#define BUFFER_LENGTH       PAGE_SIZE/8

struct list_head list_proc; /* list*/

DEFINE_SPINLOCK(sp); //Spin-lock
 

/* Nodes of the list*/
typedef struct list_item_t {
	int data;
	struct list_head links;
};


/* Nodes of the list_proc */
typedef struct list_item_proc_t {
	char *name;
	struct list_head list;
	spinlock_t sp;
	struct list_head links;
};

static struct proc_dir_entry *proc_entry;
static char *info;  // buffer_text
struct proc_dir_entry *multilist_dir=NULL;

static ssize_t multilist_read(struct file *filp, char __user *buf, size_t len, loff_t *off){

	struct list_item_proc_t* private_data=(struct list_item_proc_t*)PDE_DATA(filp->f_inode);

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
	
	spin_lock(&(private_data->sp));
	list_for_each(cur_node, &(private_data->list)) { 
			 
		item = list_entry(cur_node,struct list_item_t, links);
		printk(KERN_INFO "%i\n",item->data);	
		dest+=sprintf(dest,"%i\n",item->data);
			
	}
	spin_unlock(&(private_data->sp));



	i=dest-kbuf;
		
	if (copy_to_user(buf, kbuf,i))
		return -EFAULT;
		
	return i;	 
}

static ssize_t multilist_write(struct file *filp, const char __user *buf, size_t len, loff_t *off){

	struct list_item_proc_t* private_data=(struct list_item_proc_t*)PDE_DATA(filp->f_inode);
	int num;
	struct list_item_t* tmp=NULL;
	struct list_item_t *node = NULL;
	struct list_head *pos, *q;
	

	if (copy_from_user(info, buf, len))
		return -EFAULT;

	if (sscanf(info, "add %i", &num) == 1){
		node = (struct list_item_t *)vmalloc(sizeof(struct list_item_t));
			node->data = num;
			INIT_LIST_HEAD(&node->links);
			spin_lock(&(private_data->sp));
				list_add_tail(&node->links, &(private_data->list));
			spin_unlock(&(private_data->sp));
	}
	
	else if (sscanf(info, "remove %i", &num) == 1){
		
		spin_lock(&(private_data->sp));
		list_for_each_safe(pos, q, &(private_data->list)){
		 tmp= list_entry(pos, struct list_item_t, links);
			if (num == tmp->data){
				list_del(pos);
				vfree(tmp);
			}	 
		}
		spin_unlock(&(private_data->sp));	
	}
	else if (strncmp(info,"cleanup", 7) == 0){
		spin_lock(&(private_data->sp));
		list_for_each_safe(pos, q, &(private_data->list)){
			tmp= list_entry(pos, struct list_item_t, links);
			list_del(pos);
			vfree(tmp);
		}
		spin_unlock(&(private_data->sp));
	}
	return len;
}


static const struct file_operations proc_entry_fops_list = {
    .read = multilist_read,
    .write = multilist_write,    
};



/**
 * remove all items to de list_item
 * */
void clear_list(struct list_head* list){

	struct list_item_t* tmp=NULL;
	struct list_head *pos, *q;
	
	if (list_empty(list))
		return;
	
	list_for_each_safe(pos, q, list){
		 tmp= list_entry(pos, struct list_item_t, links);
		 list_del(pos);
		 vfree(tmp);
	}
	
}

/**
 * create proc_entry with a name into /proc/multilist/
 * */
int create_proc(const char* name){
		struct list_item_proc_t* node=NULL;
	
		node = (struct list_item_proc_t *)vmalloc(sizeof(struct list_item_proc_t));
		
		INIT_LIST_HEAD(&(node->list));
		spin_lock_init(&(node->sp));
		
		
		proc_entry = proc_create_data(name, 0666, multilist_dir, &proc_entry_fops_list, node);
		if (proc_entry == NULL ) {
			remove_proc_entry(name, NULL);
			vfree(node);
			return -ENOMEM;
		}
		node->name = vmalloc(strlen(name)+1);
		strcpy(node->name, name);
		INIT_LIST_HEAD(&node->links);
		
		
		spin_lock(&sp);
		list_add_tail(&node->links, &list_proc);
		spin_unlock(&sp);
		return 0;
}

static ssize_t control_write(struct file *filp, const char __user *buf, size_t len, loff_t *off)
{
  
	struct list_item_proc_t* tmp=NULL;
	struct list_head *pos, *q;
	
	int flag = 0;
	
    char kbuf[BUFFER_LENGTH];
    char name[BUFFER_LENGTH];
    
    if (copy_from_user(kbuf, buf, len))
		return -EFAULT;
	
	if (sscanf(kbuf, "create %s", name)){
		list_for_each_safe(pos, q, &list_proc){
			 tmp= list_entry(pos, struct list_item_proc_t, links);
			 if (!strcmp(tmp->name, name)){
				flag = 1;
				printk(KERN_INFO "Ya existe una entrada con ese nombre.\n");
				printk(KERN_INFO "tmp->name es: %s\n", tmp->name);
				printk(KERN_INFO "name es: %s\n", name);
			}
		}
		if (!flag){
			if(create_proc(name) != 0){
				return -ENOMEM;
			}
		}	
	}
	if (sscanf(kbuf, "delete %s", name)){
		
		
		spin_lock(&sp);
		list_for_each_safe(pos, q, &list_proc){
			 tmp= list_entry(pos, struct list_item_proc_t, links);
			 if (!strcmp(name, tmp->name)){
				 clear_list(&(tmp->list));
				 vfree(tmp->name);
				 list_del(pos);
				 vfree(tmp);
				 flag=1;
		 }
		}
		spin_unlock(&sp);
		
		if (flag)
			remove_proc_entry(name, multilist_dir);
		
		
	}

    return len;
}

static ssize_t control_read(struct file *filp, char __user *buf, size_t len, loff_t *off)
{
    return 0;
}

static const struct file_operations control_entry_fops = {
    .read = control_read,
    .write = control_write,
};


int init_multilist_module( void )
{
	
	INIT_LIST_HEAD(&list_proc); //initialize list
	
	info = (char *)vmalloc( BUFFER_LENGTH ); //reserve memory to buffer_text
	
	multilist_dir=proc_mkdir("multilist",NULL);
	
	/*create proc_entre /proc/multilist/, is a directory*/
	if (!multilist_dir)
		return -ENOMEM;
		
	/* Create proc entry /proc/multilist/control, No add this proc_entry to list_proc*/
    proc_entry = proc_create( "control", 0666, multilist_dir, &control_entry_fops);
    if (proc_entry == NULL ) 
        return -ENOMEM;
	
	
	/* Create proc entry /proc/multilist/default */
	if(create_proc("default") != 0){
        return -ENOMEM;
	}

    printk(KERN_INFO "Multilist loaded");

    return 0;
}




void exit_multilist_module( void )
{
	
	struct list_item_proc_t* tmp=NULL;
	struct list_head *pos, *q;

	/* loop for remove all proc_entries*/
	list_for_each_safe(pos, q, &list_proc){
		 tmp= list_entry(pos, struct list_item_proc_t, links);
		 clear_list(&(tmp->list)); //remove all items to the list_items
		 remove_proc_entry(tmp->name, multilist_dir);
		 vfree(tmp->name);
		 list_del(pos);
		 vfree(tmp); 
		 
	}
	
	remove_proc_entry("multilist", NULL);
   
    printk(KERN_INFO "multilist: Module removed.\n");
}

module_init(init_multilist_module);
module_exit(exit_multilist_module);
