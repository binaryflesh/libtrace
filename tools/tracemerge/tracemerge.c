#include <libtrace.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>
#include <getopt.h>

void usage(char *argv0)
{
	fprintf(stderr,"Usage: %s [ -i | --set-interface ] [ -u | --unique-packets ] outputuri traceuri...\n",argv0);
	fprintf(stderr,"\n");
	fprintf(stderr,"Merges traces together, with -i each trace gets it's own direction/interface,\n without traces keep whatever direction/interface they have set\n");
	exit(1);
}

int main(int argc, char *argv[])
{
	
	struct libtrace_out_t *output;
	struct libtrace_t **input;
	struct libtrace_packet_t **packet;
	bool *live;
	bool set_interface=false;
	bool unique_packets=false;
	int i=0;
	uint64_t last_ts=0;

	while (1) {
		int option_index;
		struct option long_options[] = {
			{ "set-interface", 	0, 0, 'i' },
			{ "unique-packets",	0, 0, 'u' },
			{ NULL,			0, 0, 0 },
		};

		int c=getopt_long(argc, argv, "iu",
				long_options, &option_index);

		if (c==-1)
			break;

		switch (c) {
			case 'i': set_interface=true; break;
			case 'u': unique_packets=true; break;
			default:
				fprintf(stderr,"unknown option: %c\n",c);
				usage(argv[0]);

		}

	}

	if (optind+2>argc)
		usage(argv[0]);

	output=trace_create_output(argv[optind]);
	if (trace_is_err_output(output)) {
		trace_perror_output(output,"trace_create");
		return 1;
	}
	if (trace_start_output(output)==-1) {
		trace_perror_output(output,"trace_start");
		return 1;
	}

	input=calloc((argc-optind),sizeof(struct libtrace_t *));
	packet=calloc((argc-optind),sizeof(struct libtrace_packet_t *));
	live=calloc((argc-optind),sizeof(bool));
	for(i=0;i<argc-optind;++i) {
		struct libtrace_t *f;
		f=trace_create(argv[i+optind]);
		if (trace_is_err(f)) {
			trace_perror(f,"trace_create");
			return 1;
		}
		if (trace_start(f)==-1) {
			trace_perror(f,"trace_start");
			return 1;
		}
		p=trace_create_packet();
		input[i]=f;
		packet[i]=p;
		if (trace_read_packet(f,packet[i])>0)
			live[i]=true;
	}

	while(1) {
		uint64_t oldest_ts=0;
		int oldest=-1;
		for(i=0;i<argc-2;++i) {
			if (!live[i] && input[i]) {
				int ret=trace_read_packet(input[i],packet[i]);
				if (ret<0) {
					/* Error */
					perror(argv[i+2]);
					trace_destroy(input[i]);
					input[i]=NULL;
				}
				else if (ret==0) {
					/* EOF */
					trace_destroy(input[i]);
					input[i]=NULL;
				}
				else
					live[i]=true;
			}
			if (live[i] && 
				(oldest==-1 || 
				 oldest_ts>trace_get_erf_timestamp(packet[i]))) {
				oldest=i;
				oldest_ts=trace_get_erf_timestamp(packet[i]);
			}
		}
		/* We have run out of packets! */
		if (oldest==-1) {
			break;
		}

		live[oldest]=false;

		if (set_interface)
			trace_set_direction(packet[oldest],oldest);

		if (unique_packets && oldest_ts == last_ts)
			continue;

		trace_write_packet(output,packet[oldest]);

		last_ts=oldest_ts;
		
	}
	trace_destroy_output(output);

	return 0;
}
