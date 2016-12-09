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
#include<linux/kthread.h>
#include <linux/jiffies.h> 
#include <linux/vmalloc.h>
#define DEV_MAJOR 77 //номер устройства
#define DEV_MINOR 0  //mknod потом вызываем с этим номером
#define DEV_NAME "matrix" //id устройства


MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Character device driver MATRIX");
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
static int parseInt(char *str);

#define MAXSTR 254
static int times = 0;
static char msg [MAXSTR] = {0};//тут храним данные от пользователя и результат их обработки
static char ans [MAXSTR] = {0};//тут храним данные от пользователя и результат их обработки

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

int init_module(void)
{
	int t = register_chrdev(DEV_MAJOR, DEV_NAME, &fops);

	if (t < 0)
		printk(KERN_ALERT "Device registration for Matrix Failed");
	else
		printk(KERN_ALERT "Device for Matrix registered \n");
	return t;
}

void cleanup_module(void)
{
	unregister_chrdev(DEV_MAJOR, DEV_NAME);
}

/**
 * Открытие устройства
 */
static int dev_open(struct inode * inod, struct file * fil)
{
	times++;
	printk(KERN_ALERT "Device for Matrix opened %d times\n", times);

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
	printk(KERN_ALERT "Device for Matrix Closed\n");

	return 0;
}

#define MAX_MATRIX_SIZE 1500 
int *A;
int *B;
int *R;
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

static void genRandMatrix(int *M, int size)
{
	int i,j;
	unsigned int r;
	for(i=0;i<size;i++)
		for(j=0;j<size;j++)
		{		
			get_random_bytes ( &r, sizeof (r) );
			M[i*MAX_MATRIX_SIZE+j] = r%100;
		}
}
static void printMatrix(int *M)
{
	int i,j;
	for(i=0;i<3;i++)
		printk("%d\t%d\t%d\n",M[i*MAX_MATRIX_SIZE+0],M[i*MAX_MATRIX_SIZE+1],M[i*MAX_MATRIX_SIZE+2]);
}

static void multiply(int startRow, int endRow, int size)
{
	int i,j,k;
	for(i=startRow;i<endRow;i++)
		for(j=0;j<size;j++)
		{		
			R[i*MAX_MATRIX_SIZE+j] = 0;
			for(k=0;k<size;k++)
			{		
				R[i*MAX_MATRIX_SIZE+j] += A[i*MAX_MATRIX_SIZE+k]*B[k*MAX_MATRIX_SIZE+j];
			}
		}
}

static int processCount = 1;
static int matrixSize = 3;
static struct completion finished[ MAX_MATRIX_SIZE ];
void subthread_function(void* data)
{
	int *param = (int*)data;
	printk("\tProcess for rows %d to %d started\n",param[0],param[1]);
	multiply(param[0],param[1],matrixSize);
   complete( finished + param[2] );
}
int sizes[MAX_MATRIX_SIZE][3];
static struct task_struct *processes[ MAX_MATRIX_SIZE ];
#define IDENT "for_thread_%d"
int is_first = 1;
void thread_function(void* data)
{
	int size = matrixSize < MAX_MATRIX_SIZE?matrixSize : MAX_MATRIX_SIZE;
	if (!is_first) 
	{
		vfree((void *)A);
		vfree((void *)B);
		vfree((void *)R);
	}
	A = (int*) vmalloc(matrixSize * MAX_MATRIX_SIZE  *sizeof(int));	
	B = (int*) vmalloc(matrixSize * MAX_MATRIX_SIZE  *sizeof(int));	
	R = (int*) vmalloc(matrixSize * MAX_MATRIX_SIZE  *sizeof(int));	
	is_first = 0;
	genRandMatrix(A,size);
		//printk("A:\n");
		//printMatrix(A);
	genRandMatrix(B,size);
		//printk("B:\n");
		//printMatrix(B);
	
	int k=0,i;
	for( i = 0; i < processCount; i++ )
      init_completion( finished + i );
	
	//struct timespec ts = current_kernel_time();
	long ts = jiffies;
	int step = matrixSize/processCount;
	if (step*processCount < matrixSize)
		step = matrixSize/(processCount-1);

	for (i=0;i<matrixSize;i+=step)
	{
		sizes[k][0] = i;
		sizes[k][1] = i+step < matrixSize?i+step : matrixSize;
		sizes[k][2] = k;
		//printk("\tProcess for rows %d to %d creates\n",sizes[k][0],sizes[k][1]);
		processes[k] = kthread_run(&subthread_function,(void *)sizes[k],IDENT,k);
		k++;
	}
	for (k=0;k<processCount;k++)
	{
		wait_for_completion(finished +k);
	}
	//multiply(0,size,size);
		//printk("R:\n");
		//printMatrix(R);
	//struct timespec ts1 = current_kernel_time();
	long ts1 = jiffies;
	printk("Nanosecs %lld  %lld \n",ts1,ts);
	//long ms1 = ts1.tv_sec*1000000+ts1.tv_nsec/1000;
	//long ms = ts.tv_sec*1000000+ts.tv_nsec/1000;
	printk("Task for size %d and process count %d takes %d %lld tics\n",matrixSize,processCount,ts1-ts,ts1-ts);
}
/**
 *Что-то делаем со строкой от пользователя
 */
static void process(void)
{
	if (msg[0]=='t')
	{
		genRandMatrix(A,MAX_MATRIX_SIZE);
		printk("A:\n");
		printMatrix(A);
		genRandMatrix(B,MAX_MATRIX_SIZE);
		printk("B:\n");
		printMatrix(B);
		multiply(0,3,3);
		printk("R:\n");
		printMatrix(R);
	}
	if (msg[0]=='p')
    {
			processCount = parseInt(msg+2);
	}		
	if (msg[0]=='c')
    {
			matrixSize = parseInt(msg+2);
			int data  = 1;
			struct task_struct *task;
			printk("Start task for size %d and process count %d \n",matrixSize,processCount);
			task = kthread_run(&thread_function,(void *)data,"one");			
	}	
}


