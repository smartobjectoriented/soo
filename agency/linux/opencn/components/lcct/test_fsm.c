#include <stdio.h>

typedef int (*fsm_callback_t)(void*);

#define DECLARE_FSM(name, nstates) 		\
typedef struct fsm { 					\
	fsm_callback_t rules[nstates]; 		\
	int state;							\
} fsm_t_##name

#define FSM_CB(fnname) ((fsm_callback_t)fnname)

#define FSM(name) fsm_t_##name

#define FSM_SIZE(var) (sizeof(var.rules)/sizeof(fsm_callback_t))

#define FSM_STATE_VALID(fsm_var) (fsm_var.state >= 0 && fsm_var.state < FSM_SIZE(fsm_var)) 

#define FSM_UPDATE(fsm_var, data) 															\
	if (FSM_STATE_VALID(fsm_var)) 															\
	{																						\
		if(fsm_var.rules[fsm_var.state] != NULL) {											\
			fsm_var.state = fsm_var.rules[fsm_var.state](data);								\
			if (!FSM_STATE_VALID(fsm_var)) {												\
				fprintf(stderr, "FSM: INVALID STATE RETURNED: %d\n", fsm_var.state); 		\
				fsm_var.state = -1; 														\
			}																				\
		}																					\
		else {																				\
			fprintf(stderr, "FSM: state %d has no callback\n", fsm_var.state);				\
			fsm_var.state = -1;																\
		}																					\
	}

enum {
	HOME_INIT,
	HOME_STATE1,
	HOME_STATE2,
	HOME_STATE3
};

int home_init(int* data) {
	printf("home_init\n");
	*data = 42;
	return HOME_STATE1;
}
int home_state1(int* data) {
	printf("home_state1\n");
	*data = 69;
	return HOME_STATE2;
}
int home_state2(int* data) {
	printf("home_state2\n");
	*data = -1;
	return HOME_INIT;
}


DECLARE_FSM(home, 5);
FSM(home) home_fsm = {
	.state = HOME_INIT,
	.rules = {
		[HOME_INIT] = 	FSM_CB(home_init),
		[HOME_STATE1] = FSM_CB(home_state1),
		[HOME_STATE2] = FSM_CB(home_state2)
	}
};



int main(int argc, char const *argv[])
{
	int data;
	FSM_UPDATE(home_fsm, &data);
	printf("data = %d\n", data);
	FSM_UPDATE(home_fsm, &data);
	printf("data = %d\n", data);
	FSM_UPDATE(home_fsm, &data);
	printf("data = %d\n", data);
	FSM_UPDATE(home_fsm, &data);
	printf("data = %d\n", data);
	FSM_UPDATE(home_fsm, &data);
	printf("data = %d\n", data);
	FSM_UPDATE(home_fsm, &data);
	printf("data = %d\n", data);
	FSM_UPDATE(home_fsm, &data);
	printf("data = %d\n", data);
	/* code */
	return 0;
}