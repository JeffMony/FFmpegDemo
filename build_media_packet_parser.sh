
gcc -o media_packet_parser media_packet_parser.c `pkg-config --cflags --libs libavcodec libavformat libavutil`