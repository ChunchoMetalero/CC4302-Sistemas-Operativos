#include <linux/init.h>
/* #include <linux/config.h> */
#include <linux/module.h>
#include <linux/kernel.h> /* printk() */
#include <linux/slab.h> /* kmalloc() */
#include <linux/fs.h> /* everything... */
#include <linux/errno.h> /* error codes */
#include <linux/types.h> /* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h> /* O_ACCMODE */
#include <linux/uaccess.h> /* copy_from/to_user */

#include "kmutex.h"

MODULE_LICENSE("Dual BSD/GPL");


/* Declaration of disco.c functions */
int disco_open(struct inode *inode, struct file *filp);
int disco_release(struct inode *inode, struct file *filp);
ssize_t disco_read(struct file *filp, char *buf, size_t count, loff_t *f_pos);
ssize_t disco_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos);
void disco_exit(void);
int disco_init(void);

struct file_operations disco_fops = {
  read: disco_read,
  write: disco_write,
  open: disco_open,
  release: disco_release
};

/* Declaration of the init and exit functions */
module_init(disco_init);
module_exit(disco_exit);

#define TRUE 1
#define FALSE 0

/* Buffer to store data */
#define MAX_SIZE 8192

int disco_major = 61;

struct pipe {
  char *pipebuffer;
  int in, out, size;
  KMutex mutex;
  KCondition cond;
  int closed_by_writer;
  int reader;
  int reader_finished;
};

static int readers;
static int writers;

struct pipe *savedPipe = NULL;

int disco_init(void) {
  int rc;
  rc = register_chrdev(disco_major, "disco", &disco_fops);
  if (rc < 0) {
      printk("<1>disco: cannot obtain major number %d\n", disco_major);
      return rc;
  }
  readers = 0;
  writers = 0;
  printk("<1>Inserting disco module\n");
  return 0;
}

void disco_exit(void) {
  unregister_chrdev(disco_major, "disco");
  printk("<1>Removing disco module\n");
}

int disco_open(struct inode *inode, struct file *filp) {
  printk("<1>Opening disco module\n");
  if(readers == 0 && writers == 0) {
    struct pipe *disco_pipe;
    disco_pipe = kmalloc(sizeof(struct pipe), GFP_KERNEL);
    if(disco_pipe == NULL) {
      return -ENOMEM;
    }

    disco_pipe->pipebuffer = kmalloc(MAX_SIZE, GFP_KERNEL);
    if(disco_pipe->pipebuffer == NULL) {
      kfree(disco_pipe);
      return -ENOMEM;
    }
    memset(disco_pipe->pipebuffer, 0, MAX_SIZE);
    
    disco_pipe->in = 0;
    disco_pipe->out = 0;
    disco_pipe->size = 0;
    disco_pipe->reader = 0;
    m_init(&disco_pipe->mutex);
    c_init(&disco_pipe->cond);
    disco_pipe->closed_by_writer = FALSE;
    disco_pipe->reader_finished = FALSE;

    if(filp->f_mode & FMODE_READ) {
      readers++;
    }

    if(filp->f_mode & FMODE_WRITE) {
      writers++;
    }

    savedPipe = disco_pipe;
    filp->private_data = disco_pipe;
    m_lock(&savedPipe->mutex);
    while(readers == 1 || writers == 1){
      if(c_wait(&savedPipe->cond, &savedPipe->mutex)){
        readers = (readers == 1) ? 0 : readers;
        writers = (writers == 1) ? 0 : writers;
        m_unlock(&savedPipe->mutex);
        return -EINTR;
      }
    }
  }

  if(readers == 1 || writers == 1) {
    filp->private_data = savedPipe;
  }

  if(filp->f_mode & FMODE_READ) {
    savedPipe->reader = 1;
  }
  
  readers = 0;
  writers = 0;
  c_signal(&savedPipe->cond);
  m_unlock(&savedPipe->mutex);
  return 0;
}

int disco_release(struct inode *inode, struct file *filp) {
  struct pipe *disco_pipe = filp->private_data;

  if(filp->f_mode & FMODE_READ) {
    printk("<1>Releasing disco module(reader)\n");
    if(disco_pipe->closed_by_writer == FALSE){
      m_lock(&disco_pipe->mutex);
      disco_pipe->reader_finished = TRUE;
      disco_pipe->reader = 0;
      m_unlock(&disco_pipe->mutex);
    }
  }

  if(filp->f_mode & FMODE_WRITE) {
    printk("<1>Releasing disco module(writer)\n");
    printk("El valor de reader es: %d\n", disco_pipe->reader);

    if(disco_pipe->reader == 1){
      m_lock(&disco_pipe->mutex);
      disco_pipe->closed_by_writer = TRUE;
      c_signal(&disco_pipe->cond);

      while(disco_pipe->reader_finished == FALSE){
        c_wait(&disco_pipe->cond, &disco_pipe->mutex);
      }

      kfree(disco_pipe->pipebuffer);
      m_unlock(&disco_pipe->mutex);
      kfree(disco_pipe);
    }

  }
  return 0;


}

ssize_t disco_read(struct file *filp, char *buf, size_t count, loff_t *f_pos) {
  struct pipe *disco_pipe = filp->private_data;
  int counter = count;
  m_lock(&disco_pipe->mutex);
  while(disco_pipe->size == 0){
    if(c_wait(&disco_pipe->cond, &disco_pipe->mutex)){
      printk("<1>Read interrupted\n");
      counter = -EINTR;
      goto epilog;
    }

    if(disco_pipe->closed_by_writer){
      counter = 0;
      disco_pipe->reader_finished = TRUE;
      goto epilog;
    }

  }
  if(counter > disco_pipe->size){
    counter = disco_pipe->size;
  }

  for(int i=0; i<counter; i++){
    if(copy_to_user(buf + i, disco_pipe->pipebuffer+disco_pipe->out, 1) != 0){
      counter = -EFAULT;
      goto epilog;
    }
    disco_pipe->out = (disco_pipe->out + 1) % MAX_SIZE;
    disco_pipe->size--;
  }
  printk("lo que estamos leyendo de el buffer es: %s\n", disco_pipe->pipebuffer);
epilog:
  c_signal(&disco_pipe->cond);
  m_unlock(&disco_pipe->mutex);
  return counter;
}

ssize_t disco_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos) {
  struct pipe *disco_pipe = filp->private_data;
  int counter = count;
  m_lock(&disco_pipe->mutex);
  for(int i = 0; i < counter; i++){
    while(disco_pipe->size==MAX_SIZE){
      if(c_wait(&disco_pipe->cond, &disco_pipe->mutex)){
        printk("<1>Write interrupted\n");
        counter = -EINTR;
        goto epilog;
      }
    }
    if(copy_from_user(disco_pipe->pipebuffer + disco_pipe->in, buf + i, 1) != 0){
      counter = -EFAULT;
      goto epilog;
    }
    disco_pipe->in = (disco_pipe->in + 1) % MAX_SIZE;
    disco_pipe->size++;
    c_signal(&disco_pipe->cond);
  }
  printk("lo que hay escrito en el buffer es: %s\n", disco_pipe->pipebuffer);
epilog:
  m_unlock(&disco_pipe->mutex);
  return counter;
}

