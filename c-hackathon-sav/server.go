package main

import (
	"fmt"
	"log"
	"net/http"
	"os"
)

func main() {
	port := "8000"
	if len(os.Args) > 1 {
		port = os.Args[1]
	}

	// Serve web directory
	fs := http.FileServer(http.Dir("web"))
	http.Handle("/", fs)

	addr := ":" + port
	fmt.Printf("\n╔═══════════════════════════════════════════════════════╗\n")
	fmt.Printf("║  SAV IPFIX Web Server (Go)                            ║\n")
	fmt.Printf("╠═══════════════════════════════════════════════════════╣\n")
	fmt.Printf("║  📊 Listening on http://localhost:%s                 ║\n", port)
	fmt.Printf("║  🌐 Access from: http://0.0.0.0:%s                   ║\n", port)
	fmt.Printf("╚═══════════════════════════════════════════════════════╝\n\n")

	log.Printf("Server started on port %s\n", port)
	if err := http.ListenAndServe(addr, nil); err != nil {
		log.Fatal(err)
	}
}
