#include <linux/init.h>
#include <linux/module.h>
#include <linux/timer.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <asm/uaccess.h>

#define SOURCE_PROC_NAME	"bf_source"
#define OUTPUT_PROC_NAME	"bf_output"
#define MEMORY_UNIT	char
#define INITIAL_MEMORY	1024*sizeof(MEMORY_UNIT)
#define MEMORY_INCREASE	1024*sizeof(MEMORY_UNIT)
#define SOURCE_SIZE		10240

struct timer_list hello_timer;
struct file output;

typedef struct
{
	char type;
	long quantity;
	void *loop;
	void *next;
} Instruction;

MEMORY_UNIT *memory, *limit, *p;
Instruction *program;

int die(char *msg)
{
	printk(KERN_ERR "Error: %s\n", msg);
	return -EFAULT;
}

void init_mem(size_t size)
{
	memory = (MEMORY_UNIT *)kmalloc(size, GFP_KERNEL);
	if(memory == NULL)
		die("Cannot allocate memory");
	limit = memory + size;
	memset(memory, 0, size);
}

void expand_memory(size_t size)
{
	size_t memsize = limit - memory + size;
	size_t p_relative = p - memory;

	memory = (MEMORY_UNIT *)krealloc(memory, memsize, GFP_KERNEL);
	if(memory == NULL)
		die("Cannot allocate memory");
	limit = memory + memsize;
	p = p_relative + memory;
	memset(limit - size, 0, size);
}

Instruction *load(char buf[], int j)
{
	char c;
	long l;
	int len;
	Instruction *i = (Instruction *)kmalloc(sizeof(Instruction), 
						GFP_KERNEL);
	Instruction *first = i;
	len = sizeof(buf) / sizeof(buf[0]);
	while (j < len)
	{
		c = buf[j];
		j++;
		i->type = c;
		l = 1;
		switch (c)
		{
			case '[':
				i->loop = load(buf, j);
				break;
			case ']':
				return first;
			case '<': case '>': case '+': case '-': case ',': case '.':
				if (j < len)
				{
					while (j + 1 < len && buf[j + 1] == c)
					{
						l++;
						j++;
					}
					j--;
				}
				i->quantity = l;
				break;
			default:
				continue;
		}
		i->next = (Instruction *)kmalloc(sizeof(Instruction), 
						GFP_KERNEL);
		i = i->next;
	}
	i->type = ']';
	return first;
}

void execute(Instruction *i);

void run(Instruction *list)
{
	while (list->type != ']')
	{
		execute(list);
		list = list->next;
	}
}

void execute(Instruction *i)
{
	long l;

	switch (i->type)
	{
		case '>':
			p += i->quantity;
			while(p >= limit) expand_memory(MEMORY_INCREASE);
			return;
		case '<':
			if (p - i->quantity < memory)
				die("Negative memory pointer");
			p -= i->quantity;
			return;
		case '+':
			*p += i->quantity;
			return;
		case '-':
			*p -= i->quantity;
			return;
		case '.':
			for (l=0; l<i->quantity; l++)
			{
				if (output.f_op->write(&output, p, sizeof(*p), &output.f_pos))
					die("Cannot write result to procfs");
			}
			return;
		case ',':
			for (l=0; l<i->quantity; l++) /* ここでinputの読み込み */;
			return;
		case '[':
			while (*p) run(i->loop);
			return;
		default:
			return;
	}
}

void free_list(Instruction *list)
{
	Instruction *next;
	while (list->type != ']')
	{
		next = list->next;
		if (list->type == '[') free_list(list->loop);
		kfree(list);
		list = next;
	}
	kfree(list);
}

static loff_t output_lseek(struct file *file, loff_t offset, int origin)
{
	output = *file;
	return seq_lseek(file, offset, origin);
}

static ssize_t source_proc_write(struct file *file, const char __user *buffer,
				size_t count, loff_t *pos)
{
	int n;
	char *buf;
	buf = (char *)kmalloc(SOURCE_SIZE, GFP_KERNEL);
	if(buf == NULL)
		die("Cannot allocate memory");
	n = (count < SOURCE_SIZE - 1)?
		count: SOURCE_SIZE - 1;
	if (copy_from_user(buf, buffer, n))
		die("Cannot read source code");
	buf[n] = '\0';
	init_mem(INITIAL_MEMORY);
	p = memory;
	load(buf, 0);
	run(program);
	free_list(program);
	kfree(memory);
	return count;
}

static const struct file_operations source_proc_fops = {
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = source_proc_write,
};

static const struct file_operations output_proc_fops = {
	.llseek = output_lseek,
	.release = single_release,
};

static int bf_init(void)
{
	proc_create(SOURCE_PROC_NAME, S_IWUSR|S_IRUSR, NULL, &source_proc_fops);
	proc_create(OUTPUT_PROC_NAME, S_IRUGO, NULL, &output_proc_fops);
	printk(KERN_DEBUG "test!\n");
	return 0;
}

static void bf_exit(void)
{
	remove_proc_entry(SOURCE_PROC_NAME, NULL);
	remove_proc_entry(OUTPUT_PROC_NAME, NULL);
	printk(KERN_DEBUG "good bye kernel!\n");
}

module_init(bf_init);
module_exit(bf_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Hiromu Yakura");
MODULE_DESCRIPTION("A brainfxxk kernel module");

