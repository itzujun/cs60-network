#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/select.h>
#include <dirent.h>
#include <sys/stat.h>

#include <sys/inotify.h>

#include "event_queue.h"
#include "inotify_utils.h"
#include "../common/filetable.h"
#include "../common/constants.h"
#include "../peer/peer.h"

char wds[MAX_MONITOR_DIR][FILE_NAME_MAX_LEN];

extern int keep_running;
extern int inotify_fd;
static int watched_items;
extern char my_ip[IP_LEN];


/* Create an inotify instance and open a file descriptor
   to access it */
int open_inotify_fd ()
{
  int fd;

  watched_items = 0;
  fd = inotify_init ();

  if (fd < 0)
    {
      perror ("inotify_init () = ");
    }
  return fd;
}


/* Close the open file descriptor that was opened with inotify_init() */
int close_inotify_fd (int fd)
{
  int r;

  if ((r = close (fd)) < 0)
    {
      perror ("close (fd) = ");
    }

  watched_items = 0;
  return r;
}

Node* createNode(char *cur_event_filename){
	      struct stat mystat;
	      bzero(&mystat, sizeof(struct stat));
	      if(stat(cur_event_filename, &mystat)<0){
		      perror("stat failure");
		      return NULL;
	      }
	      Node * newEntry = (Node * )malloc(sizeof (Node));
	      if(newEntry == NULL) return NULL;
	      bzero(newEntry,sizeof(Node));
	      strcpy(newEntry->name,cur_event_filename );
	      newEntry->size = mystat.st_size;
	      FILE * fp;
	      char buffer[80],cmd[80];
	      strcpy(cmd,"/usr/bin/md5sum");
	      strcat(cmd," ");
	      strcat(cmd,cur_event_filename);
	      if((fp = popen(cmd, "r") )<0)
			      return NULL;
	      fgets(buffer,sizeof(buffer),fp);
	      printf("%s",buffer);
	      pclose(fp);

	      memcpy(newEntry->md5,buffer,MD5_LEN);
	      memcpy(newEntry->newpeerip, my_ip,IP_LEN);
	      newEntry->peer_ip_num=1;
	      return newEntry;
}

/* This method does the work of determining what happened,
   then allows us to act appropriately
*/
void handle_event (queue_entry_t event)
{
  /* If the event was associated with a filename, we will store it here */
  char cur_event_filename [FILE_NAME_MAX_LEN];
  char *cur_event_file_or_dir = NULL;
  /* This is the watch descriptor the event occurred on */
  int cur_event_wd = event->inot_ev.wd;
  //int cur_event_cookie = event->inot_ev.cookie;
  unsigned long flags;

  if (event->inot_ev.len)
    {
      if(event->inot_ev.name[0]=='.')return;
      if(strstr(event->inot_ev.name,".swp")!=NULL)return;
      if(event->inot_ev.name[strlen(event->inot_ev.name)-1] =='~' )return;
      strcpy(cur_event_filename,wds[cur_event_wd]);
      strcat(cur_event_filename,event->inot_ev.name);
     
    }
  if ( event->inot_ev.mask & IN_ISDIR )
    {
	 //   if(event->inot_ev.mask & !(IN_CREATE|IN_DELETE))
	//	    return;
      	   cur_event_file_or_dir = "Dir";
    }
  else 
    {
      cur_event_file_or_dir = "File";
    }
  flags = event->inot_ev.mask & 
    ~(IN_ALL_EVENTS | IN_UNMOUNT | IN_Q_OVERFLOW | IN_IGNORED );

  /* Perform event dependent handler routines */
  /* The mask is the magic that tells us what file operation occurred */
  switch (event->inot_ev.mask & 
	  //(IN_ALL_EVENTS | IN_UNMOUNT | IN_Q_OVERFLOW | IN_IGNORED))
	  (IN_MODIFY | IN_CREATE | IN_DELETE|IN_CLOSE_WRITE|IN_ATTRIB|IN_IGNORED))
    {
      /* File was accessed */
    /*case IN_ACCESS:
      printf ("ACCESS: %s \"%s\" on WD #%i\n",
	      cur_event_file_or_dir, cur_event_filename, cur_event_wd);
      break;
*/
      /* Subdir or file was deleted */
    case IN_DELETE:
      printf ("DELETE: %s \"%s\" on WD #%i\n",
	      cur_event_file_or_dir, cur_event_filename, cur_event_wd);
      if(strcmp(cur_event_file_or_dir, "File") == 0){
      	deleteFileEntry(cur_event_filename);
        sendFileUpdate();
      }
      break;

      /* Subdir or file was created */
    case IN_CREATE:
      printf ("CREATE: %s \"%s\" on WD #%i\n",
	      cur_event_file_or_dir, cur_event_filename, cur_event_wd);
      if(strcmp(cur_event_file_or_dir,"Dir") == 0){
	      watch_dir(inotify_fd,cur_event_filename,IN_ALL_EVENTS);
      }else{
	      Node* newEntry = createNode(cur_event_filename);
	      if(newEntry!=NULL){
	      		appendFileEntry(newEntry);
			sendFileUpdate();
	      }
	      //free(newEntry);
      }
      break;
      /* File was modified */
    case IN_MODIFY:
      printf ("MODIFY: %s \"%s\" on WD #%i\n",
	      cur_event_file_or_dir, cur_event_filename, cur_event_wd);
      
      break;

      /* File changed attributes */
    case IN_ATTRIB:
      printf ("ATTRIB: %s \"%s\" on WD #%i\n",
	      cur_event_file_or_dir, cur_event_filename, cur_event_wd);
      break;

      /* File open for writing was closed */
    case IN_CLOSE_WRITE:
      printf ("CLOSE_WRITE: %s \"%s\" on WD #%i\n",
	      cur_event_file_or_dir, cur_event_filename, cur_event_wd);
      if(strcmp(cur_event_file_or_dir,"File") == 0){
	      Node* newEntry = createNode(cur_event_filename);
	      if(newEntry!=NULL){
		      Node* np = findFileEntryByName(newEntry->name);
	              if(np == NULL || (np != NULL && strcmp(np->md5,newEntry->md5) == 0))
			    return;
	      	      updateFileEntry(np, newEntry);
		      sendFileUpdate();
	      	      free(newEntry);
	      }
      }
      break;

      /* File open read-only was closed */
  /*  case IN_CLOSE_NOWRITE:
      printf ("CLOSE_NOWRITE: %s \"%s\" on WD #%i\n",
	      cur_event_file_or_dir, cur_event_filename, cur_event_wd);
      break;
*/
      /* File was opened */
  /*  case IN_OPEN:
      printf ("OPEN: %s \"%s\" on WD #%i\n",
	      cur_event_file_or_dir, cur_event_filename, cur_event_wd);
      break;
*/
      /* File was moved from X */
  /*  case IN_MOVED_FROM:
      printf ("MOVED_FROM: %s \"%s\" on WD #%i. Cookie=%d\n",
	      cur_event_file_or_dir, cur_event_filename, cur_event_wd, 
              cur_event_cookie);
      break;
*/
      /* File was moved to X */
  /*  case IN_MOVED_TO:
      printf ("MOVED_TO: %s \"%s\" on WD #%i. Cookie=%d\n",
	      cur_event_file_or_dir, cur_event_filename, cur_event_wd, 
              cur_event_cookie);
      break;
*/

      /* Watched entry was deleted */
    /*case IN_DELETE_SELF:
      printf ("DELETE_SELF: %s \"%s\" on WD #%i\n",
	      cur_event_file_or_dir, cur_event_filename, cur_event_wd);
      break;
*/
      /* Watched entry was moved */
  /*  case IN_MOVE_SELF:
      printf ("MOVE_SELF: %s \"%s\" on WD #%i\n",
	      cur_event_file_or_dir, cur_event_filename, cur_event_wd);
      break;
*/
      /* Backing FS was unmounted */
  /*  case IN_UNMOUNT:
      printf ("UNMOUNT: %s \"%s\" on WD #%i\n",
	      cur_event_file_or_dir, cur_event_filename, cur_event_wd);
      break;
*/
      /* Too many FS events were received without reading them
         some event notifications were potentially lost.  */
  /*  case IN_Q_OVERFLOW:
      printf ("Warning: AN OVERFLOW EVENT OCCURRED: \n");
      break;
*/
      /* Watch was removed explicitly by inotify_rm_watch or automatically
         because file was deleted, or file system was unmounted.  */
    case IN_IGNORED:
      watched_items--;
      printf ("IGNORED: WD #%d\n", cur_event_wd);
      printf("Watching = %d items\n",watched_items); 
      break;

      /* Some unknown message received */
    default:
      //printf ("UNKNOWN EVENT \"%X\" OCCURRED for file \"%s\" on WD #%i\n",
	//      event->inot_ev.mask, cur_event_filename, cur_event_wd);
      break;
    }
  /* If any flags were set other than IN_ISDIR, report the flags */
  if (flags & (~IN_ISDIR))
    {
      flags = event->inot_ev.mask;
//      printf ("Flags=%lX\n", flags);
    }
}

void handle_events (queue_t q)
{
  queue_entry_t event;
  while (!queue_empty (q))
    {
      event = queue_dequeue (q);
      handle_event (event);
      free (event);
    }
}

int read_events (queue_t q, int fd)
{
  char buffer[16384];
  size_t buffer_i;
  struct inotify_event *pevent;
  queue_entry_t event;
  ssize_t r;
  size_t event_size, q_event_size;
  int count = 0;

  r = read (fd, buffer, 16384);
  if (r <= 0)
    return r;
  buffer_i = 0;
  while (buffer_i < r)
    {
      /* Parse events and queue them. */
      pevent = (struct inotify_event *) &buffer[buffer_i];
      event_size =  offsetof (struct inotify_event, name) + pevent->len;
      q_event_size = offsetof (queue_entry, inot_ev.name) + pevent->len;
      event = malloc (q_event_size);
      memmove (&(event->inot_ev), pevent, event_size);
      queue_enqueue (event, q);
      buffer_i += event_size;
      count++;
    }
  //printf ("\n%d events queued\n", count);
  return count;
}

int event_check (int fd)
{
  fd_set rfds;
  FD_ZERO (&rfds);
  FD_SET (fd, &rfds);
  /* Wait until an event happens or we get interrupted 
     by a signal that we catch */
  return select (FD_SETSIZE, &rfds, NULL, NULL, NULL);
}

int process_inotify_events (queue_t q, int fd)
{
  while (keep_running && (watched_items > 0))
    {
      if (event_check (fd) > 0)
	{
	  int r;
	  r = read_events (q, fd);
	  if (r < 0)
	    {
	      break;
	    }
	  else
	    {
	      handle_events (q);
	    }
	}
    }
  return 0;
}

int watch_dir (int fd, char *dirname, unsigned long mask)
{
  int wd;
  wd = inotify_add_watch (fd, dirname, mask);
  if (wd < 0)
    {
      printf ("Cannot add watch for \"%s\" with event mask %lX", dirname,
	      mask);
      fflush (stdout);
      perror (" ");
    }
  else
    {
      watched_items++;
      bzero(wds[wd],FILE_NAME_MAX_LEN);
      strcpy(wds[wd],dirname);
      if(wds[wd][strlen(wds[wd])-1]!='/'){
	      strcat(wds[wd],"/");
      }
      printf ("Watching %s WD=%d\n", wds[wd], wd);
      printf ("Watching = %d items\n", watched_items); 
    }

	struct dirent *ent = NULL;  
	DIR *pDir;  
	if((pDir = opendir(dirname)) != NULL)  
	{  
		while(NULL != (ent = readdir(pDir)))  
		{ 
			char curdir[100];
			strcpy(curdir,dirname);
      			if(curdir[strlen(curdir)-1] != '/'){
	      			strcat(curdir,"/");
      			}
			strcat(curdir,ent->d_name);
			if(ent->d_type == 8)      {           // d_type：8-文件，4-目录  
			//	free(newEntry);
	      			Node* newEntry = createNode(curdir);
	      			if(newEntry!=NULL){
	      				appendFileEntry(newEntry);
				}
			}
			else if(ent->d_name[0] != '.')  
			{  

				//printf("\n[Dir]:\t%s\n", ent->d_name);  
				watch_dir(fd,curdir,mask);                   // 递归遍历子目录  
			}  
		}  
		closedir(pDir);  
	}  

  return wd;
}

int ignore_wd (int fd, int wd)
{
  int r;
  r = inotify_rm_watch (fd, wd);
  if (r < 0)
    {
      perror ("inotify_rm_watch(fd, wd) = ");
    }
  else 
    {
      watched_items--;
    }
  return r;
}
