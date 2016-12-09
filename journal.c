#include <linux/module.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/reboot.h>
#include <linux/unistd.h>
#include <asm/uaccess.h>
#include <linux/time.h>
#include <linux/random.h>
#include <linux/syscalls.h>
#include <linux/kernel.h>
#include <linux/sched.h> 
#include <linux/rbtree.h>
#include <linux/spinlock.h>
#include<linux/kthread.h>
#include <linux/jiffies.h> 
#include <linux/vmalloc.h>
#include <linux/delay.h>

#define DEV_MAJOR 78 //номер устройства
#define DEV_MINOR 0  //mknod потом вызываем с этим номером
#define DEV_NAME "onto" //id устройства


MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Character device driver JOURNAL");
MODULE_AUTHOR("Frog");

/**
 * Прототипы
 */

static int dev_open(struct inode *, struct file *);
static int dev_rls(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);
static void process(void);
static void clearMemory(void);
static int compare(char* s1, char *s2);

#define MAXSTR 254
static int times = 0;
static char msg [MAXSTR] = {0};//тут храним данные от пользователя и результат их обработки
static char ans [MAXSTR] = {0};//тут храним данные от пользователя и результат их обработки
static int aspirantsCount = 0;
static int professorsCount = 0;
static int journalsCount = 0;
static struct rb_root mytreeChannels = RB_ROOT;
static struct rb_root mytreeProcesses = RB_ROOT;

//------------------------------------------------------------------------------------------------------

static int compare(char* s1, char *s2)
{
	int i=0;
	for(;(s1[i]!='\n' && s1[i]!='\0') || (s2[i]!='\n' && s2[i]!='\0');i+=2)
	{
		if(s1[i]>s2[i] || (s1[i]==s2[i] && s1[i+1]>s2[i+1]))
		{
			return 1;
		}
		else if (s1[i]<s2[i] || (s1[i]==s2[i] && s1[i+1]<s2[i+1]))
		{
			return -1;
		}
	}
	if ((s1[i]!='\n' && s1[i]!='\0'))
		return 1;
	if ((s2[i]!='\n' && s2[i]!='\0'))
		return -1;
	
	return 0;
}
//------------------------------------------------------------------------------------------------------
  struct Message {
    char from[10];
    char type;
    char author[10];
    int result;
  };
  struct myChannelType {
  	struct rb_node node;
	  spinlock_t read_lock;
	  spinlock_t write_lock;
	  struct Message value;	
	  char keystring[256];
  };
  struct myChannelType *channel_search(struct rb_root *root, char *string)
  {
  	struct rb_node *node = root->rb_node;

  	while (node) {
  		struct myChannelType *data = container_of(node, struct myChannelType, node);
		int result;

		result = compare(string, data->keystring);

		if (result < 0)
  			node = node->rb_left;
		else if (result > 0)
  			node = node->rb_right;
		else
  			return data;
	}
	return NULL;
  }
  int channel_insert(struct rb_root *root, struct myChannelType *data)
  {
  	struct rb_node **new = &(root->rb_node), *parent = NULL;

  	/* Figure out where to put new node */
  	while (*new) {
  		struct myChannelType *this = container_of(*new, struct myChannelType, node);
  		int result = compare(data->keystring, this->keystring);

		parent = *new;
  		if (result < 0)
  			new = &((*new)->rb_left);
  		else if (result > 0)
  			new = &((*new)->rb_right);
  		else
  			return 0;
  	}

  	/* Add new node and rebalance tree. */
  	rb_link_node(&data->node, parent, new);
  	rb_insert_color(&data->node, root);

	return 1;
  }
//------------------------------------------------------------------------------------------------------
  struct myProcessType {
  	struct rb_node node;
	  struct task_struct *descriptor;	
	  char keystring[256];
  };
  struct myProcessType *process_search(struct rb_root *root, char *string)
  {
  	struct rb_node *node = root->rb_node;

  	while (node) {
  		struct myProcessType *data = container_of(node, struct myProcessType, node);
		int result;

		result = compare(string, data->keystring);

		if (result < 0)
  			node = node->rb_left;
		else if (result > 0)
  			node = node->rb_right;
		else
  			return data;
	}
	return NULL;
  }
  int my_process_insert(struct rb_root *root, struct myProcessType *data)
  {
  	struct rb_node **new = &(root->rb_node), *parent = NULL;

  	/* Figure out where to put new node */
  	while (*new) {
  		struct myProcessType *this = container_of(*new, struct myProcessType, node);
  		int result = compare(data->keystring, this->keystring);

		parent = *new;
  		if (result < 0)
  			new = &((*new)->rb_left);
  		else if (result > 0)
  			new = &((*new)->rb_right);
  		else
  			return 0;
  	}

  	/* Add new node and rebalance tree. */
  	rb_link_node(&data->node, parent, new);
  	rb_insert_color(&data->node, root);

	return 1;
  }
//------------------------------------------------------------------------------------------------------
  static void clearMemory(void)
  {
        struct rb_node *node;
        for (node = rb_first(&mytreeChannels); node; node = rb_next(node))
        {
          struct myClannelType *data = rb_entry(node, struct myChannelType, node);
          rb_erase(node, &mytreeChannels);
          vfree(data);
        }
        for (node = rb_first(&mytreeProcesses); node; node = rb_next(node))
        {
          struct myProcessType *dataP = rb_entry(node, struct myProcessType, node);
          kthread_stop(dataP->descriptor);
          rb_erase(node, &mytreeProcesses);
          vfree(dataP);
        }
  }
//------------------------------------------------------------------------------------------------------

/**
 * Структура для связи
 */

static struct file_operations fops =
{
	.read = dev_read,
	.open = dev_open,
	.write = dev_write,
	.release = dev_rls
};

static char *transform[2];
int init_module(void)
{
	int t = register_chrdev(DEV_MAJOR, DEV_NAME, &fops);

	if (t < 0)
		printk(KERN_ALERT "Device registration for Journal Failed");
	else
		printk(KERN_ALERT "Device for Journal registered \n");
	return t;
}

void cleanup_module(void)
{
  struct rb_node *node;
  clearMemory();
	unregister_chrdev(DEV_MAJOR, DEV_NAME);
}

/**
 * Открытие устройства
 */
static int dev_open(struct inode * inod, struct file * fil)
{
	times++;
	printk(KERN_ALERT "Device for Onto opened %d times\n", times);

	return 0;
}

/**
 * Обработчик чтения данных с устройства
 */
static ssize_t dev_read(struct file * filp, char * buff, size_t  len, loff_t * off)
{

	 int sz = len >= strlen(ans) ? strlen(ans) : len;

	    if (*off >= strlen(ans))
	        return 0;

	    if (copy_to_user(buff, ans, sz))
	        return -EFAULT;

	    *off += sz;//коррекция числа обработанных данных: иначе будет все-время отдавать  ту-же строку с начала
		ans[0]='\0';
	    return len;

}

/**
 * Обработчик записи в устройство
 */
static ssize_t dev_write(struct file * fil, const char * buff, size_t len, loff_t * off)
{
	unsigned long ret;
	printk(KERN_INFO "dev write \n");
	if (len > sizeof(msg) - 1)
	    return -EINVAL;
	ret = copy_from_user(msg, buff, len);
	if (ret)
	    return -EFAULT;
	msg[len] = '\0';
	process();//обработка после получения тут
	return len;
}

/**
 * Закрытие устройства
 */
static int dev_rls(struct inode * inod, struct file * fil)
{
	printk(KERN_ALERT "Device for Journal Closed\n");

	return 0;
}

static int parseInt(char *str)
{
	int res = 0;
	int i=0;
	for (; str[i] <= '9' && str[i] >= '0';i++)
	{
		res = res*10+(str[i]-'0');
	}
	return res;
}



static int isEquals(char* s1, char *s2)
{
	int i=0,k;
	for(;(s1[i]!='\n' && s1[i]!='\0') || (s2[i]!='\n' && s2[i]!='\0');i+=2)
	{
		if(s1[i]!=s2[i] || s1[i+1]!=s2[i+1])
		{
			return 0;
		}
	}
	return 1;
}

//-----------------------------------------------------------------------------------------


static void aspirant(void *data)
{
  int id = ((int) data);
  printk("Aspirant %d start to work\n",id);
  char name[10];
  sprintf(name, "a_%d",id);
  char my_ans_name[10]; 
  sprintf(my_ans_name,"%s_ans",name);
	  printk("0\n");

  struct myChannelType *ansChannel = (struct myChannelType*)vmalloc(sizeof(struct myChannelType));
  printk("1\n");
  spin_lock_init(&(ansChannel->read_lock));
  printk("2\n");
  spin_lock_init(&(ansChannel->write_lock));
  printk("3\n");
  spin_lock(&(ansChannel->read_lock));
  printk("4\n");
  //spin_unlock(&(ansChannel->write_lock));
  printk("5\n");
  memcpy(ansChannel->keystring,my_ans_name,sizeof(my_ans_name));
  printk("6\n");
  channel_insert(&mytreeChannels,ansChannel);
  while(!kthread_should_stop()) {
    if (professorsCount == 0)
    {
      printk("Aspirant %d: there is no professors in my country!\n",id);
      msleep(10000);
		schedule();
      continue;
    }
    /*unsigned int r;
    get_random_bytes ( &r, sizeof (r) );
    r %= professorsCount;
    // Задаем вопрос
    printk("Aspirant %d: I ask professor %d!\n",id,r);
    char channel_name[10]; 
    sprintf(channel_name,"p_%d",r);
    struct myChannelType *channel = channel_search(&mytreeChannels,channel_name);
    struct Message q;
    memcpy(q.from,name,sizeof(name));
    q.type='a';
    memcpy(q.author,name,sizeof(name));

    spin_lock(&(channel->write_lock));
    memcpy(&(channel->value),&q,sizeof(q));
    spin_unlock(&(channel->read_lock));
        
    // Ждем ответ
    printk("Aspirant %d: I wait for answer of professor %d!\n",id,r);
    spin_lock(&(ansChannel->read_lock));
    memcpy(&q,&(ansChannel->value),sizeof(q));
    spin_unlock(&(ansChannel->write_lock));
    printk("Aspirant %d: Answer of professor %d is !\n",id,r,q.result);
   */
    schedule();
  }
}

static void professor(void *data)
{
  int id = *((int*) data);
  printk("Professor %d start to work, and now drink tea\n",id);
  char name[10];
  sprintf(name, "p_%d",id);
  struct myChannelType *ansChannel = (struct myChannelType*)vmalloc(sizeof(struct myChannelType));
  spin_lock_init(&(ansChannel->read_lock));
  spin_lock_init(&(ansChannel->write_lock));
  spin_lock(&(ansChannel->read_lock));
  spin_unlock(&(ansChannel->write_lock));
  memcpy(ansChannel->keystring,name,sizeof(name));
  channel_insert(&mytreeChannels,ansChannel);  
  while(!kthread_should_stop()) {
    // читаем пакеты и обрабатываем их
    struct Message q;
    printk("Professor %d: I drink tea...\n",id);
    spin_lock(&(ansChannel->read_lock));
    memcpy(&q,&(ansChannel->value),sizeof(q));
    spin_unlock(&(ansChannel->write_lock));
    char channel_name[10]; 
    switch (q.type)
    {
      case 'a': 
        {
          sprintf(channel_name,"%s_ans",q.from);
          struct myChannelType *channel = channel_search(&mytreeChannels,channel_name);
          struct Message q;
          memcpy(q.from,name,sizeof(name));
          q.type='a';
          memcpy(q.author,name,sizeof(name));
          unsigned int r;
          get_random_bytes ( &r, sizeof (r) );
          r %= 2;
          q.result = r;
          spin_lock(&(channel->write_lock));
          memcpy(&(channel->value),&q,sizeof(q));
          spin_unlock(&(channel->read_lock));
        }
        break;
    }
    schedule();
  }
}

/**
 *Что-то делаем со строкой от пользователя
 */

static void process(void)
{
  struct myProcessType *process;
  int data;
  char name[10];
  switch (msg[0])
  {
    case 'a':
      data = aspirantsCount++;
      
      sprintf(name, "a_%d",data);
      process = (struct myProcessType*)vmalloc(sizeof(struct myProcessType));
      process->descriptor = kthread_run(&aspirant,(void *)(data),name);
	  printk("Aspirant %d added to system.\n",data);  
      memcpy(process->keystring,name,sizeof(name));
      my_process_insert(&mytreeProcesses,process); 
      break;
    case 'p':
      data = professorsCount++;
      sprintf(name, "p_%d",data);
      process = (struct myProcessType*)vmalloc(sizeof(struct myProcessType));
      process->descriptor = kthread_run(&professor,(void *)(&data),name);
      memcpy(process->keystring,name,sizeof(name));
      my_process_insert(&mytreeProcesses,process); 
      break;
    case 'j':break;
    case 'e':
        clearMemory();
      break;
  }
}


