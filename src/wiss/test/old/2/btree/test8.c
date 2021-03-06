#include <wiss.h>
#include <st.h>	

#define	SIDOFFSET	0
#define	SNAMEOFFSET	SIDOFFSET + sizeof(int)
#define	SSEXOFFSET	SNAMEOFFSET + 50
#define	SAGEOFFSET	SSEXOFFSET + 2
#define	SYROFFSET	SAGEOFFSET + sizeof(int)
#define SGPAOFFSET	SYROFFSET + sizeof(int)
#define	SRECLENGTH	34

/* error handlers */
#define	IOERROR(p,c)	if((int)(c)<0) \
		{printf("%s %s\n", p, io_error(c));io_final(); exit(-1);}
#define	BFERROR(p,c)	if((int)(c)<0) \
	{printf("%s %s\n",p,bf_error(c));bf_final(); io_final(); exit(-1);}
#define	ERROR(p,c)	if((int)(c)<0) printf("%s %s\n", p, st_error((int)(c)))
#define	STERROR(p,c)	if((int)(c)<0) {ERROR(p, c); st_final(); bf_final();\
				io_final();}


/* wiss file name */
#define	TESTFILE	"student"

/* print student record */
#define	PRINTST(student)\
	printf("(ID=%3d,Age=%2.2d,Year=%d,Sex=%c,",\
		student.id, student.age, student.yr,\
		student.sex[0] == 'm' ? 'M' : 'F');\
	printf("GPA=%4.2f,Name=\"%-20.20s\")\n",student.gpa, student.name)

/* student record format (for the wiss file) */
typedef	struct
	{
		int 	id;	
		char 	name[50];
		char 	sex[2];
		int 	age;		
		int 	yr;
		float 	gpa;
	} studentrec;

static	KEYINFO	keyattr[] ={ 	{ SIDOFFSET, sizeof(int), TINTEGER},
				{ SNAMEOFFSET, 30, TSTRING},
				{ SGPAOFFSET, sizeof(float), TFLOAT},
			   };

static	KEY	key[] ={ 	{ TINTEGER, sizeof(int), "\0"},
				{ TSTRING, 1, "n"},
				{ TFLOAT, sizeof(float), "\0"},
			};

/************************************************************************
**				test8.c
*************************************************************************/

main(argc, argv)
int	argc;	
char	**argv;
{
	int	errorcode;		/* for returned errors */
	int	vol;			/* volume id */
	int	i;

	/* warm up procedure for wiss */
	wiss_checkflags(&argc,&argv);
	i = io_init();			/* initialize level 0 */
	IOERROR("test8/io_init", i);
	
	i = bf_init();		/* initialize level 1 */
	BFERROR("test8/bf_init", i);

	i = st_init();			/* initialize level 2 */
	STERROR("test8/st_init", i);

	/* mount the volume call "wiss2" */
	vol = st_mount("wiss2");	
	STERROR("test8/st_mount", vol);

/* step 1. create an empty data file */
	printf(" creating an empty data file %s\n", TESTFILE);
	errorcode = st_createfile(vol, TESTFILE, 6, 90,90);
	STERROR("test8/st_createfile", errorcode);

/* step 2. load the data base -- fill the data file */
	printf(" loading the database ... \n");
	load_database(vol, TESTFILE);
#ifdef	BUFTRACE
	BF_dumpfixed();	/* check if any buffer fixed in the buffer pool */
#endif

	/* RETRIEVE FROM (TESTFILE) */
	printf(" retrieving all records ... \n");
	retrieve(vol, TESTFILE, NULL);
#ifdef	BUFTRACE
	BF_dumpfixed();	/* check if any buffer fixed in the buffer pool */
#endif

/* step 3. create 3 index files directly (from bottom up) */

	printf(" creating three index files in batch\n");
	errorcode = st_createindex(vol, TESTFILE, 1, 
			&keyattr[0], 100, FALSE, FALSE);
	STERROR("test8/st_createindex", errorcode);
	errorcode = st_createindex(vol, TESTFILE, 2, 
			&keyattr[1], 100, FALSE, FALSE);
	STERROR("test8/st_createindex", errorcode);
	errorcode = st_createindex(vol, TESTFILE, 3, 
				&keyattr[2], 100, FALSE, FALSE);
	STERROR("test8/st_createindex", errorcode);
#ifdef	BUFTRACE
	BF_dumpfixed();	/* check if any buffer fixed in the buffer pool */
#endif

	printf(" three index scans \n");

	printf(" the key of the first scan is %d \n", 50);
	*(int *)key[0].value = 50;
	errorcode = dump_rids(vol, TESTFILE, 1, &key[0]);
	STERROR("test8/st_indexscan", errorcode);

	printf(" the key of the second scan is %c \n", key[1].value[0]);
	errorcode = dump_rids(vol, TESTFILE, 2, &key[1]);
	STERROR("test8/st_indexscan", errorcode);

	printf(" the key of the third scan (on GPA) is %4.2f \n", 3.0);
	*(float *)key[2].value = 3.0;
	errorcode = dump_rids(vol, TESTFILE, 3, &key[2]);
	STERROR("test8/st_indexscan", errorcode);
#ifdef	BUFTRACE
	BF_dumpfixed();	/* check if any buffer fixed in the buffer pool */
#endif

/* step 7. delete all indices and drop the index files */

	errorcode = st_dropbtree(vol, TESTFILE, 1);
	STERROR("test8/st_dropbtree", errorcode);
	errorcode = st_dropbtree(vol, TESTFILE, 2);
	STERROR("test8/st_dropbtree", errorcode);
	errorcode = st_dropbtree(vol, TESTFILE, 3);
	STERROR("test8/st_dropbtree", errorcode);
#ifdef	BUFTRACE
	BF_dumpfixed();	/* check if any buffer fixed in the buffer pool */
#endif

/* clear up */
	/* remove the file since the test in done */
	errorcode = st_destroyfile(vol, TESTFILE);
	STERROR("test8/st_destroyfile", errorcode);

	/* dismount the volume call "wiss2" */
	errorcode = st_dismount("wiss2");  
	STERROR("test8/st_dismount", errorcode);
#ifdef	BUFTRACE
	BF_dumpfixed();	/* check if any buffer fixed in the buffer pool */
#endif

}

retrieve(vol, filename)
int 	vol;		/* volume id */
char	*filename;	/* where the records are to be retrieved */
{
	int	errorcode;		/* for returned error codes */
	int 	openfile_number;	/* open file number of the wiss file */
	RID	currid;
	studentrec student;		/* buffer for student records */

	/* open the wiss file with READ permission */
	openfile_number = st_openfile(vol, filename, READ);
	STERROR("test8/st_openfile", openfile_number);


	/* do a sequential scan, and print out those qualified records */
	for ( errorcode = st_firstfile(openfile_number, &currid); 
					errorcode >= eNOERROR;)
	{
		/* read in the qualified record */
		errorcode = st_readrecord(openfile_number, &currid,
				(char *)&student, sizeof(studentrec));
		STERROR("test8/st_readrecord", errorcode);

		/* print student record */
		PRINTST(student);

		/* set cursor to the next qualified record */
		errorcode = st_nextfile(openfile_number, &currid, &currid);

	}
	printf("\n");

	errorcode = st_closefile(openfile_number);
	STERROR("test8/st_closefile", errorcode);

}

load_database(vol, filename)
int	vol;		/* volume id */
char	*filename;	/* wiss file to store the database */
{

#define	INPUT	"studentdata/student"

	int	i;			/* loop index */
	int	errorcode;		/* for returned error codes */
	int	unixfile;		/* unix file id */
	int	openfile_number;	/* open file number of the wiss file */
	char	buf[100];		/* buffer for records in unix file */
	RID	*newrid;		/* RID of the newly created record */
	studentrec	student;	/* buffer for records in wiss file  */

	/* open the wiss file with WRITE permission */
	openfile_number = st_openfile(vol, filename, WRITE);
	STERROR("test8/st_openfile", openfile_number);

	/* open the unix file first */
	unixfile = open(INPUT, 0);
	if (unixfile < 0)
	{
		printf("\ncan not open student data file");
		return;
	}

	/* read records from the unix file and put them into the wiss file */
	while (read(unixfile, buf, SRECLENGTH))
	{
		/* convert the record format */
		sscanf(buf,"%d", &student.id);
		for (i = 0; i < 20; i++)
			student.name[i] = buf[3 + i]; 		
		sscanf(&buf[23], "%c %2d %d %f", 
			 student.sex, &student.age, &student.yr, &student.gpa);

		/* insert it into the wiss file */
		errorcode = st_insertrecord(openfile_number, 
			&student, sizeof(studentrec), NULL, &newrid);
		STERROR("test8/st_insertrecord", errorcode);
	}


	/* close the wiss file */
	errorcode = st_closefile(openfile_number);
	STERROR("test8/st_closefile", errorcode);

}

dump_rids(vol, filename, index, key)
int 	vol;		/* volume id */
char	*filename;	/* where the records are to be retrieved */
int	index;		/* index # of the index file */
KEY	*key;
{
	register	e;
	int		openfilenum;
	int		indexofn;
	studentrec	student;
	RID		rid;
	XCURSOR		cursor;
	int		i;

	indexofn = st_openbtree(vol, filename, index, READ);
	CHECKERROR(indexofn);

	openfilenum = st_openfile(vol, filename, READ);
	CHECKERROR(openfilenum);

	/* forward dump */
	printf(" *** FORWARD DUMP ***\n");
	e = st_firstindex(indexofn, key, &cursor, &rid);
	for ( i = 0 ; e >= eNOERROR; i++)
	{
		e = st_readrecord(openfilenum, &rid, (char *)&student, 
			sizeof(studentrec));
		CHECKERROR(e);

		PRINTST(student);
		e = st_getadjrid(indexofn, NEXT, &cursor, &rid);
	}

printf(" \n%d records found\n", i);

	/* backward dump */
	printf(" *** BACKWARD DUMP ***\n");
	e = st_lastindex(indexofn, key, &cursor, &rid);
	for ( i = 0; e >= eNOERROR; i++)
	{
		e = st_readrecord(openfilenum, &rid, (char *)&student, 
			sizeof(studentrec));
		CHECKERROR(e);

		PRINTST(student);
		e = st_getadjrid(indexofn, PREV, &cursor, &rid);
	}

printf(" %d records found\n", i);

	e = st_closefile(indexofn);
	CHECKERROR(e);
	e = st_closefile(openfilenum);
	CHECKERROR(e);

}
