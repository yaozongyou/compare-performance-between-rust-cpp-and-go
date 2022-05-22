package main

import (
	"flag"
	"fmt"
	"log"
	"net/http"
)

var (
	bindingAddress = flag.String("binding_address", "0.0.0.0:3000", "binding address")
)

func main() {
	flag.Parse()

	http.HandleFunc("/greeting", func(w http.ResponseWriter, r *http.Request) {
		name := r.URL.Query().Get("name")
		fmt.Fprintf(w, "Hello %s", name)
	})

	log.Fatal(http.ListenAndServe(*bindingAddress, nil))
}
