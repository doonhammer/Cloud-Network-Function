# Overview
This is the test directory for repository.
Tests can be run with two different configurations, one is a simple chain of veths adn the other sets up a Linux Bridge. The two configurations are shown below.


# veth chain Setup

┌───────────────────────────────┐                ┌───────────────────────────────┐                 ┌───────────────────────────────┐
│                               │                │                               │                 │                               │
│                               │                │                               │                 │                               │
│                               │                │                               │                 │                               │
│                               │                │                               │                 │                               │
│                               │                │                               │                 │                               │
│       Client Namespace        │                │         VNF Namespace         │                 │       Server Namespace        │
│                               │                │                               │                 │                               │
│                               │                │                               │                 │                               │
│          ┌──────────┐         │                │ ┌──────────┐    ┌──────────┐  │                 │        ┌──────────┐           │
│          │  veth0   │         │                │ │  veth1   │    │  veth2   │  │                 │        │  veth3   │           │
│          │          │         │                │ │          │    │          │  │                 │        │          │           │
└──────────┴──────────┴─────────┘                └─┴──────────┴────┴──────────┴──┘                 └────────┴──────────┴───────────┘
                │                                        ▲               │                                         ▲                
                │                                        │               │                                         │                
                └────────────────────────────────────────┘               └─────────────────────────────────────────┘                

# Linux Bridge Setup

┌───────────────────────────────┐                ┌───────────────────────────────┐                 ┌───────────────────────────────┐
│                               │                │                               │                 │                               │
│                               │                │                               │                 │                               │
│                               │                │                               │                 │                               │
│                               │                │                               │                 │                               │
│                               │                │                               │                 │                               │
│       Client Namespace        │                │         VNF Namespace         │                 │       Server Namespace        │
│                               │                │                               │                 │                               │
│                               │                │                               │                 │                               │
│          ┌──────────┐         │                │          ┌──────────┐         │                 │        ┌──────────┐           │
│          │  veth0   │         │                │          │  veth1   │         │                 │        │  veth2   │           │
│          │          │         │                │          │          │         │                 │        │          │           │
└──────────┴──────────┴─────────┘                └──────────┴──────────┴─────────┘                 └────────┴──────────┴───────────┘
                │                                                │                                                 ▲                
                │                                                │                                                 │                
                │                                                │                                                 │                
                │                                                │                                                 │                
                │                                                │                                                 │                
                │                                                │                                                 │                
                └────────────────────────────────────────────────┼─────────────────────────────────────────────────┘                
                                                                 │                                                                  
                                                                 │                                                                  
                                                                 │                                                                  
                                                                 │                                                                  
                                                                 │                                                                  
                                                                 ▼                                                                  
                                             ┌──────────────────────────────────────┐                                               
                                             │                                      │                                               
                                             │                                      │                                               
                                             │                                      │                                               
                                             │             Linux Bridge             │                                               
                                             │                                      │                                               
                                             │                                      │                                               
                                             │                                      │                                               
                                             └──────────────────────────────────────┘                                               


# Running Tests


To get help on the tests use the following command

```
$ ./runtests -h
```

To get the list of available tests

```
$ ./runtests -l
```

To run a specific test

```
$ ./runtests -t "test-number"
```

