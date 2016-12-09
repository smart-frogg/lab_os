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
#include <linux/vmalloc.h>
#include <linux/suspend.h>

#define DEV_MAJOR 76 //номер устройства
#define DEV_MINOR 0  //mknod потом вызываем с этим номером
#define DEV_NAME "onto" //id устройства


MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Character device driver ONTO");
MODULE_AUTHOR("Frog");

/**
 * Прототипы
 */


static int dev_open(struct inode *, struct file *);
static int dev_rls(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);
static int getProcessesCount(void);
static void process(void);
static int readConfig(void);
static void readLine(void);
static int parseInt(char *str);
static int compare(char* s1, char *s2);

#define MAXSTR 254
static int times = 0;
static char msg [MAXSTR] = {0};//тут храним данные от пользователя и результат их обработки
static char ans [MAXSTR] = {0};//тут храним данные от пользователя и результат их обработки
static struct rb_root mytreeDictionary = RB_ROOT;

//------------------------------------------------------------------------------------------------------
  struct mytype {
  	struct rb_node node;
	int value;	
	char keystring[256];
  };
  struct mytype *my_search(struct rb_root *root, char *string)
  {
  	struct rb_node *node = root->rb_node;

  	while (node) {
  		struct mytype *data = container_of(node, struct mytype, node);
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
  int my_insert(struct rb_root *root, struct mytype *data)
  {
  	struct rb_node **new = &(root->rb_node), *parent = NULL;

  	/* Figure out where to put new node */
  	while (*new) {
  		struct mytype *this = container_of(*new, struct mytype, node);
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
		printk(KERN_ALERT "Device registration for Onto Failed");
	else
		printk(KERN_ALERT "Device for Onto registered \n");
	readConfig();
	transform[0]="йцукенгшщзхъфывапролджэячсмитьбю";
	transform[1]="ЙЦУКЕНГШЩЗХЪФЫВАПРОЛДЖЭЯЧСМИТЬБЮ";
	return t;
}

void cleanup_module(void)
{
  struct rb_node *node;
  for (node = rb_first(&mytreeDictionary); node; node = rb_next(node))
  {
	  struct mytype *data = rb_entry(node, struct mytype, node);
	  rb_erase(node, &mytreeDictionary);
	  vfree(data);
  }
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
	printk(KERN_ALERT "Device for Onto Closed\n");

	return 0;
}


#define MAX_COUNT 20
#define LINE_BUF_LEN 255

#define QUESTIONS_COUNT 10
#define UNDERSTAND_COUNT 4

static int dictionaryCount = 0;

//static char dictionary[MAX_COUNT][LINE_BUF_LEN+1];
//static int dictionaryValues[MAX_COUNT];
static int dontUnderstandCount = 0;
static char dontUnderstand[MAX_COUNT][LINE_BUF_LEN+1];
static int questionCount = 0;
static char question[MAX_COUNT][LINE_BUF_LEN+1];
static int questionYNCount = 0;
static char questionYN[MAX_COUNT][LINE_BUF_LEN+1];
static int yesCount = 0;
static char yes[MAX_COUNT][LINE_BUF_LEN+1];
static int noCount = 0;
static char no[MAX_COUNT][LINE_BUF_LEN+1];



int evaluation = 0;
int prevEvaluation = 0;

#define ANS_GOOD 1
#define ANS_BAD 2
#define ANS_SLEEP 3
#define ANS_SWITCH_OFF 4

//-------------------------------------------------------------------------
/**
 * Состояние
 */
static int eSum=0;
static int pSum = 0;
static int countE = 0;
static void put(int e, int p)
{
	eSum += e;
	pSum += p;
	countE++;
}
static int getAvgE(void)
{
	return eSum/countE;
}
static int getAvgP(void)
{
	return pSum/countE;
}
//-------------------------------------------------------------------------
/**
 * Чтение конфига
 */
static char* file = NULL; 
module_param( file, charp, 0 ); 

#define BUF_LEN 2047 
#define DEFNAME "/home/varvara/Документы/mymodule/onto.conf";  // произвольный текстовый файл 
static char buff[ BUF_LEN + 1 ] = DEFNAME; 
static int buffRealSize;
static int pointer = 0;

static char lineBuff[LINE_BUF_LEN + 1];

static void readArray(int *forCount, char array[MAX_COUNT][LINE_BUF_LEN + 1])
{
	readLine();
	*forCount = parseInt(lineBuff);
	int i;
	for(i=0;i<*forCount;i++)
	{
		readLine();
		memcpy(array[i],lineBuff,LINE_BUF_LEN+1);
	}
}

static int readConfig( void ) { 
    struct file *f; 

    if( file != NULL ) strcpy( buff, file ); 
    printk( "*** openning file: %s\n", buff ); 
    f = filp_open( buff, O_RDONLY, 0 ); 

    if( IS_ERR( f ) ) { 
        printk( "*** file open failed: %s\n", buff ); 
        return -ENOENT; 
    } 

    buffRealSize = kernel_read( f, 0, buff, BUF_LEN ); 
    if( buffRealSize ) { 
        printk( "*** read first %d bytes\n", buffRealSize ); 
        buff[ buffRealSize ] = '\0';
		readLine();
		dictionaryCount = parseInt(lineBuff);
		printk( "dictionary сount: %d\n", dictionaryCount );
		int i;
		struct mytype *data;
		for(i=0;i<dictionaryCount;i++)
		{
			printk( "READ %d STRING\n", i ); 
			readLine();
			data = (struct mytype*)vmalloc(sizeof(struct mytype));
			data->value = parseInt(lineBuff);
			memcpy(data->keystring,lineBuff+2,LINE_BUF_LEN-1);
			my_insert(&mytreeDictionary, data);
			//dictionaryValues[i] = parseInt(lineBuff);
			//memcpy(dictionary[i],lineBuff+2,LINE_BUF_LEN-1);
		}
		
  struct rb_node *node;
		  for (node = rb_first(&mytreeDictionary); node; node = rb_next(node))
			printk("key=%s\n", rb_entry(node, struct mytype, node)->keystring);
		readArray(&dontUnderstandCount,dontUnderstand);
		readArray(&questionCount,question);
		readArray(&questionYNCount,questionYN);
		readArray(&yesCount,yes);
		readArray(&noCount,no);
    } else { 
        printk( "*** kernel_read failed\n" ); 
        return -EIO; 
    } 
    printk( "*** close file\n" ); 
    filp_close( f, NULL ); 
    return -EPERM; 
} 

static void readLine(void)
{
	int i=pointer;
	for (; i<buffRealSize && buff[i] != '\n'; i++);
	memcpy(lineBuff,buff+pointer,i-pointer);
	lineBuff[i-pointer] = '\0';
	pointer = i+1;
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


static int getHours(void)
{
	struct timespec ts;
	getnstimeofday(&ts);
	int hour = (long long)ts.tv_sec/(3600);
	hour %= 24;
	return hour+3;
}

static void Hello(void)
{
	char  hi1 [MAXSTR] = "Доброе утро!" ;
	char  hi2 [MAXSTR] = "Добрый день!" ;
	char  hi3 [MAXSTR] = "Добрый вечер!" ;
	int hour = getHours();
	if (hour >= 5 && hour < 12)
		strncpy(ans, strcat(ans,hi1), MAXSTR);
	else if (hour < 5 || hour > 22)
		strncpy(ans, strcat(ans, hi3), MAXSTR);
	else
		strncpy(ans, strcat(ans, hi2), MAXSTR);
}

static void Ask(void)
{
	unsigned int r;
	get_random_bytes ( &r, sizeof (r) );
	r %= questionCount;
	printk("Номер вопроса %d. ",r);
	//sprintf(msg,"%d",r);
	strncpy(ans, strcat(ans, question[r]), MAXSTR);
	//strncpy(msg, strcat(msg, " [y/n]\n"), MAXSTR);	
}

static void AskYN(void)
{
	unsigned int r;
	get_random_bytes ( &r, sizeof (r) );
	r %= questionYNCount;
	printk("Номер вопроса %d. ",r);
	strncpy(ans, strcat(ans, questionYN[r]), MAXSTR);
}

static void DontUnderstand(void)
{
	unsigned int r;
	get_random_bytes ( &r, sizeof (r) );
	r %= dontUnderstandCount;
	strncpy(ans, dontUnderstand[r], MAXSTR);
	//Hello();
	Ask();
}

static void toLowerCase(void)
{
	int i=0,k;
	for(;msg[i]!='\n' && msg[i]!='\0' ;i+=2)
	{
		for(k=0;k<33*2;k+=2)
			if(transform[1][k]==msg[i] && transform[1][k+1]==msg[i+1])
			{
				msg[i] = transform[0][k];
				msg[i+1] = transform[0][k+1];
			}
	}
	msg[i] = '\0';
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

static int compare(char* s1, char *s2)
{
	int i=0,k;
	for(;(s1[i]!='\n' && s1[i]!='\0') || (s2[i]!='\n' && s2[i]!='\0');i+=2)
	{
		if(s1[i]>s2[i] || s1[i]==s2[i] && s1[i+1]>s2[i+1])
		{
			return 1;
		}
		else if (s1[i]<s2[i] || s1[i]==s2[i] && s1[i+1]<s2[i+1])
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

#define HELLO_STAGE 1
#define ANSWER_STAGE 2
#define YN_ANSWER_STAGE 3
#define REBOOT_STAGE 4
#define SLEEP_STAGE 5

static int stage = HELLO_STAGE;

static void ProcessAnswer(void)
{
	int i=0;
	toLowerCase();
	int good = 0;
	/*for(;i<dictionaryCount;i++)
	{
		if(isEquals(msg,dictionary[i]))
		{
			prevEvaluation = evaluation;
			evaluation = dictionaryValues[i];
			good = 1;
			break;
		}
	}*/
	struct mytype *data = my_search(&mytreeDictionary, msg);
	if(data)
	{
			prevEvaluation = evaluation;
			evaluation = data->value;
			good = 1;
	}
	if(!good)
	{
		DontUnderstand();
		return;
	}
	if(prevEvaluation !=0)
	{
		if(prevEvaluation > evaluation)
			strncpy(ans, strcat(ans, "Что, хуже, чем в прошлый раз, да? "), MAXSTR);
		else if(prevEvaluation < evaluation)
			strncpy(ans, strcat(ans, "Полегчало с прошлого раза, да? "), MAXSTR);
		else
			strncpy(ans, strcat(ans, "Что, опять? "), MAXSTR);
	}
	
	if (countE > 3)
		if (getAvgE() > evaluation)
		{
			strncpy(ans, strcat(ans, "Обычно-то по-лучше... "), MAXSTR);
		}
		else if (getAvgE() < evaluation)
		{
			strncpy(ans, strcat(ans, "Ну, обычно всё хуже... "), MAXSTR);
		}
	
	int procCount = getProcessesCount();
	if(evaluation<3 && procCount >195)
	{
		strncpy(ans, strcat(ans, "Я тоже устал. Отрубаемся! "), MAXSTR);
		stage = REBOOT_STAGE;
	}
	else if(evaluation>=3 && procCount >195)
	{
		strncpy(ans, strcat(ans, "А я, между прочим, устал... Отрубаемся! "), MAXSTR);
			stage = REBOOT_STAGE;
	}
	else if(evaluation>=3 && procCount <195)
		strncpy(ans, strcat(ans, "Давай каких-нибудь прог запустим, а то мне скучно! "), MAXSTR);
	else 
	{
		strncpy(ans, strcat(ans, "Ну давай еще поработаем... "), MAXSTR);
		stage = YN_ANSWER_STAGE;
		AskYN();
	}
	put(evaluation, procCount);
}

static void ProcessYNAnswer(void)
{
	int i=0;
	int good=0;
	int answer;
	toLowerCase();
	for(;i<yesCount;i++)
	{
		if(isEquals(msg,yes[i]))
		{
			good = 1;
			answer = 1;
			break;
		}
	}
	if(!good)
	{
		for(;i<noCount;i++)
		{
			if(isEquals(msg,no[i]))
			{
				good = 1;
				answer = -1;
				break;
			}
		}
	}
	
	if(!good)
	{		
		DontUnderstand();
		return;
	}
	if (answer==1)
	{
		strncpy(ans, strcat(ans, "Тогда я подожду? "), MAXSTR);
		stage = SLEEP_STAGE;
	}

}
static int getProcessesCount(void)
{
	int i=0;
	struct task_struct *task;
	for_each_process(task) {
		i++;
    	//printk(KERN_INFO "Process %i is named %s\n", task->pid, task->comm); 
	}
   return i;
}

/**
 *Что-то делаем со строкой от пользователя
 */

static void process(void)
{
	ans[0] = 0;
	switch (stage)
	{
		case HELLO_STAGE: 
			printk("%s. Приветствую и задаю вопрос.",msg);
			Hello(); 
			Ask(); 
			stage = ANSWER_STAGE; 
			break;
		case ANSWER_STAGE:
			printk("%s. Обрабатываю ответ.",msg);
			stage = HELLO_STAGE;
			ProcessAnswer();
			break;
		case YN_ANSWER_STAGE:
			printk("%s. Обрабатываю ответ.",msg);
			stage = ANSWER_STAGE;
			ProcessYNAnswer();
			break;
		case REBOOT_STAGE: 
			printk("Отключаемся.");
			/*_reboot(LINUX_REBOOT_MAGIC1, 
           LINUX_REBOOT_MAGIC2, 
           LINUX_REBOOT_CMD_POWER_OFF, 0);
			//pm_power_off();*/
			kernel_power_off();
			break;
		case SLEEP_STAGE: 
			printk("Спааать.");
			//pm_suspend(PM_SUSPEND_MAX);
			stage = HELLO_STAGE; 
			break;
			
		default: DontUnderstand(); break;
	}
	
}


