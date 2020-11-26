# Synchronization
Synchronization and execution of a job scheduing program using threads

Generally, in a complex system, jobs are submitted for execution and get scheduled according to the system's efficiency goals. The system that has been implemented handles a large volume of incoming jobs by using several admission queues to store jobs before they get scheduled into execution.

In this system, each job needs specific resources and must be granted exclusive access to those resources before it can execute. Once a job has access to its necessary resources, it completes its execution and relinquishes access to them. So the execution system ensures correct usage of its resources. It also ensures that while a job has access to a resource, no other job can acquire the same resource, otherwise the system will operate incorrectly.

A job is assigned to a specific processor by the execution system according to some predefined policy. This policy is typically quite complex in real systems, and requires dynamic knowledge of operational parameters of the system, such as system load. For this program an static processor assignment policy was used instead for simplicity.

For analysis purposes, the system also keeps track of various data about its operation, such as which jobs completed on which processors, and how many jobs utilized each of its resources.
