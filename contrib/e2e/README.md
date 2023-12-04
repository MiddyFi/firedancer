# Firedancer End-to-End Tests

The ansible playbooks here can setup a full Solana Labs/Firedancer cluster and run
various tests against the cluster to simulate real world scenarios.

## Pre-requisites

Passwordless SSH access needs to be setup between the Ansible controller node
and the hosts specified in the inventory.

## Development

Create a `inventory/dev.yml` file similar to `inventory/hosts.yml` and populate
it with host information.  Then you can run with `-i inventory/dev.yml`.  If
you want to use a different ansible user, specify that with `-e "ansible_user=<USER>"` to the
`ansible-playbook` command. For example:

```
ansible-playbook -i inventory/dev.yml playbooks/build.yml -e "ansible_user=<USER>"
```

## Running

Create a `inventory/dev.yml` file similar to `inventory/hosts.yml` and populate
it with host information. The grouping should be as follows:

* `all.hosts` - all hosts must be in here
* `all.children.[fd|labs]_build.hosts` - firedancer and solana build hosts respectively, should have only 1 host
* `all.children.genesis.hosts` - cluster bootstrap node, should have only 1 host
* `all.children.[fd|labs]_run.hosts` - firedancer and solana validator hosts, include the bootstrap node in the appropriate group here

### Building the binaries

```
ansible-playbook -i inventory/dev.yml playbooks/build.yml -e "ansible_user=<USER>"
```

### Bootstrapping the cluster

```
ansible-playbook -i inventory/dev.yml playbooks/genesis_and_bootstrap.yml -e "ansible_user=<USER> override_keygen=true"
```

### Joining other nodes

Labs validators:

```
ansible-playbook -i inventory/dev.yml playbooks/labs_join.yml -e "ansible_user=<USER> override_keygen=true"
```

Firedancer validators:

```
ansible-playbook -i inventory/dev.yml playbooks/fd_join.yml -e "ansible_user=<USER> override_keygen=true"
```

### Tearing everything down

```
ansible-playbook -i inventory/dev.yml playbooks/teardown.yml -e "ansible_user=<USER>"
```

## To Dos

### Higher Priority

- [ ] Firedancer as the bootstrap validator
- [ ] Framework for tests: running, adding, evaluating

**Tests**

- [ ] Transfers through all validators
- [ ] Bench TPS against the cluster
- [ ] Smart contract execution
- [ ] NFT mint
- [ ] Crashing tiles and restart times

### Lower Priority

- [ ] Handle errors during teardown and make sure teardown is successful
- [ ] Allow joining validators without `override_keygen`
- [ ] Group validator joining logic into some `common` role
- [ ] Larger playbook for doing everything one click
- [ ] Add checks for inventory definitions
