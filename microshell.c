#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

//FOR DEBUG PRINT//
#include <stdio.h>
//_______________//

#define NONE 0
#define PIPE 1

typedef struct		c_cmd
{
	int				type;
	char 			**arguments;
	int 			filedes[2];
	struct c_cmd	*next;
	struct c_cmd	*prev;
}					t_cmd;

int ft_strlen(char *str)
{
	int len = 0;
	while (str[len])
		len++;
	return (len);
}

void ft_putstr(char *str)
{
	write(2, str, ft_strlen(str));
}

void exit_fatal()
{
	ft_putstr("error: fatal\n");
	exit(1);
}

char *ft_strdup(char *str)
{
	char *new;
	int i = 0;
	int len = ft_strlen(str);
	new = (char *)malloc((sizeof(char) * (len + 1)));
	while (str[i])
	{
		new[i] = str[i];
		i++;
	}
	new[i] = '\0';
	return (new);
}

void clear_cmd(t_cmd *cmd)
{
	t_cmd	*tmp;
	int		i;

	while (cmd)
	{
		i = 0;
		while (cmd->arguments[i++])
			free(cmd->arguments[i]);
		free(cmd->arguments);
		tmp = cmd;
		cmd = cmd->next;
		free(tmp);
	}
}

t_cmd *new_command(t_cmd *prev, char **arguments, int num_of_arguments, int type)
{
	int i = 0;
	t_cmd *new_command = NULL;

	new_command = (t_cmd *)malloc(sizeof(t_cmd));
	new_command->prev = prev;
	if (prev)
		prev->next = new_command;
	new_command->next = NULL;
	new_command->type = type;
	new_command->arguments = (char **)malloc(sizeof(char *) * (num_of_arguments + 1));
	while (i < num_of_arguments)
	{
		new_command->arguments[i] = ft_strdup(arguments[i]);
		i++;
	}
	new_command->arguments[i] = NULL;
	return (new_command);
}

int exec_cd(t_cmd *cmd)
{
	int i = 0;
	int res = 0;
	while (cmd->arguments[i])
		i++;
	if (i != 2)
	{
		ft_putstr("error: cd: bad arguments\n");
		return (1);
	}
	res = chdir(cmd->arguments[1]);
	if (res < 0)
	{
		ft_putstr("error: cd: cannot change directory to ");
		ft_putstr(cmd->arguments[1]);
		ft_putstr("\n");
	}
	return (res);
}

int exec_non_builtin(t_cmd *cmd, char **envs)
{
	pid_t	pid;
	int 	status;
	int 	res = 0;

	if (cmd->type == PIPE)
		if (pipe(cmd->filedes) < 0)	// Creating a pipe. It gives us a set of two file descriptors, filedes[0] and filedes[1]. 
			// Anything written to filedes[1] can be read from filedes[0].
			exit_fatal();

	pid = fork();
	if (pid < 0)
		exit_fatal();
	if (pid == 0)	//	 Child process goes here
	{
		if (cmd->type == PIPE && dup2(cmd->filedes[STDOUT_FILENO], STDOUT_FILENO) < 0)	// Now we need to change the standard output of the process 
			// to the writing end of the pipe (filedes[1]). 
			// We use the dup2() system call to duplicate the writing file descriptor of the
			// pipe (filedes[1]) onto the standard output file descriptor, 1.
			exit_fatal();
		if (cmd->prev && cmd->prev->type == PIPE && dup2(cmd->prev->filedes[STDIN_FILENO], STDIN_FILENO) < 0)	// Here, we need to change the standard input of the process 
			// to the reading end of the pipe (filedes[0]). 
			// We use the dup2() system call to duplicate the reading file descriptor of the
			// pipe (filedes[0]) onto the standard input file descriptor, 0.
			exit_fatal();
		res = execve(cmd->arguments[0], cmd->arguments, envs);
		if (res < 0)
		{
			ft_putstr("error: cannot execute ");
			ft_putstr(cmd->arguments[0]);
			ft_putstr("\n");
		}
		exit (res);
	}
	else			//	 Parent process goes here
	{
		waitpid(pid, &status, 0);
		if (WIFEXITED(status))
			res = WEXITSTATUS(status);
		if (cmd->type == PIPE)
		{
			close(cmd->filedes[STDOUT_FILENO]);			// Closing the output end of the pipe (filedes[1])
			if (!cmd->next)
				close(cmd->filedes[STDIN_FILENO]);		// Closing the input end of the pipe (filedes[0])
		}
		if (cmd->prev && cmd->prev->type == PIPE)
			close(cmd->prev->filedes[STDIN_FILENO]);	// Closing the input end (filedes[0]) of the previous pipe
	}
	return(res);
}

int exec(t_cmd *cmd, char **envs)
{
	int res = 0;
	while (cmd)
	{
		if (!strcmp(cmd->arguments[0], "cd"))
			res = exec_cd(cmd);
		else
			res = exec_non_builtin(cmd, envs);
		cmd = cmd->next;
	}
	return (res);
}

//_______________FOR DEBUG PRINT_______________//
void debug_print_command_list(t_cmd *cmd)
{
	int		i = 1;
	int		arg;

	while (cmd)
	{
		printf("__________________\n");
		printf("COMMAND \x1b[32m#%d\x1b[0m\n", i);
		if (cmd->type == NONE)
			printf("TYPE == NONE\n");
		if (cmd->type == PIPE)
			printf("TYPE == PIPE\n");
		printf("ARGUMENTS:\n");
		arg = 0;
		while (cmd->arguments[arg])
		{
			printf("[%s] ", cmd->arguments[arg]);
			arg++;
		}
		printf("\n__________________\n\n");
		cmd = cmd->next;
		i++;
	}
	printf("   \x1b[32mRESULT\x1b[0m   \n");
	printf(" \x1b[32m |||||||| \x1b[0m\n");
	printf(" \x1b[32m VVVVVVVV \x1b[0m\n");
}
//_____________________________________________//


int main(int argc, char **argv, char **envs)
{
	int		res = 0;
	int		first = 1;
	int		last = 0;
	int		type;
	t_cmd	*cmd = NULL;
	t_cmd	*tmp = NULL;

	while (++last < argc)
	{
		if (!strcmp(argv[last], "|") || !strcmp(argv[last], ";") || last + 1 == argc)
		{
			if (!strcmp(argv[last], "|"))
				type = PIPE;
			else if (!strcmp(argv[last], ";"))
				type = NONE;
			else
			{
				type = NONE;
				last++;
			}
			if (last - first != 0)
			{
				tmp = new_command(tmp, &argv[first], last - first, type);
				if (cmd == NULL)
					cmd = tmp;
			}
			first = last + 1;
		}
	}
	//_______FOR DEBUG PRINT_______//
	debug_print_command_list(cmd);
	//_____________________________//
	res = exec(cmd, envs);
	clear_cmd(cmd);
	return (res);
}
