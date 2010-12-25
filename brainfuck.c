#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>

#define BF_SOURCE	"bf_source"
#define MEMORY_UNIT	char
#define BF_SOURCE_SIZE	10240
#define BF_OUTPUT_SIZE	10240
#define BF_MEMORY_SIZE	30000

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Hiromu Yakura");

void execute(const char* source)
{
	MEMORY_UNIT *mem;
	char *output;
	int ptr = 0, len = 0, i = 0, count, source_len;
	source_len = sizeof(source) / sizeof(char);
	mem = kmalloc(sizeof(MEMORY_UNIT) * BF_MEMORY_SIZE, GFP_KERNEL);
	if(mem == NULL) {
		printk(KERN_ERR "Memory Error.\n");
		return;
	}
	output = kmalloc(sizeof(char) * BF_OUTPUT_SIZE, GFP_KERNEL);
	memset(mem, 0, sizeof(mem));
	while (source[i] != '\0') {
		if(source[i] == '>' && ptr < BF_SOURCE_SIZE - 1) {
			ptr++;
		} else if(source[i] == '<' && ptr > 0) {
			ptr--;
		} else if(source[i] == '+') {
			mem[ptr]++;
		} else if(source[i] == '-') {
			mem[ptr]--;
		} else if(source[i] == '.') {
			output[len] = mem[ptr];
			len++;
		} else if(source[i] == ',') {
			// Input
		} else if(source[i] == '[') {
			if(!mem[ptr]) {
				count = 1;
				while(1) {
					i++;
					if(source[i] == '[') {
						count++;
					} else if(source[i] == ']') {
						count--;
						if(!count)
							break;
					} else if(source[i] == '\0' || i > BF_SOURCE_SIZE) {
						printk(KERN_ERR "Source code invalid.\n");
						return;
					}
				}
			}
		} else if(source[i] == ']') {
			count = 1;
			while(1) {
				i--;
				if(source[i] == ']') {
					count++;
				} else if(source[i] == '[') {
					count--;
					if(count == 0)
						break;
				} else if(i < 0) {
					printk(KERN_ERR "Source code invalid.\n");
					return;
				}
			}
			i--;
		}
		i++;
	}
	output[len] = '\0';
	printk(KERN_INFO "BF: %s\n", output);
}

static int proc_write(struct file *file, const char *buf, unsigned long count, void *data)
{
	int len;
	char *source;
	source = kmalloc(sizeof(char) * BF_SOURCE_SIZE, GFP_KERNEL);
	if(source == NULL) {
		printk(KERN_ERR "Memory Error.\n");
		return -EFAULT;
	}
	len = (count < BF_SOURCE_SIZE - 1)? count: BF_SOURCE_SIZE - 1;
	if (copy_from_user(source, buf, len)) {
		return -EFAULT;
	}
	source[len] = '\0';
	execute(source);
	return count;
}

static int __init bf_init(void)
{
	struct proc_dir_entry *proc;
	proc = create_proc_entry(BF_SOURCE, 0666, NULL);
	if (proc) {
		proc->write_proc = proc_write;
	} else {
		printk(KERN_ERR "Create proc was failed.\n");
		return -EFAULT;
	}
	return 0;
}

static void __exit bf_exit(void)
{
	remove_proc_entry(BF_SOURCE, NULL);
}

module_init(bf_init);
module_exit(bf_exit);
