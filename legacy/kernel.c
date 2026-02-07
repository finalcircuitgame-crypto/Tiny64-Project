void kernel_main() {
    const char *str = "Tiny64 OS initialized!";
    char *vidptr = (char*)0xb8000;
    int i = 0;
    int j = 0;

    // Clear screen
    while(j < 80 * 25 * 2) {
        vidptr[j] = ' ';
        vidptr[j+1] = 0x07;
        j = j + 2;
    }

    // Print string
    j = 0;
    while(str[i] != '\0') {
        vidptr[j] = str[i];
        vidptr[j+1] = 0x07;
        i++;
        j = j + 2;
    }

    while(1);
}
