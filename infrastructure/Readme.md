# Setup for Azure

## Prerequsites

- Azure CLI installed
- Kubernetes CLI installed
- Terraform installed

## Install Kubernetes

First install the Azure CLI and log in.

Create a file `terraform.tfvars` in the infrastructure folder and enter the
access data to the service principal that will authenticate the AKS instance:

```
appId    = "...your app id here..."
password = "...your password here..."
```

Terraform will use these credentials to provision resources on Azure.

After you've done this, initalize your Terraform workspace, which will download
the provider and initialize it with the values provided in the `terraform.tfvars` file.

```shell
$ terraform init
```

Then, provision your AKS cluster by running `terraform apply`. This will
take approximately 10 minutes and then produce an error before starting
to install the K8S resources.

```shell
$ terraform apply
```

To download your AKS credentials execute the `get-k8s-credentials.sh`
script and then execute `terraform apply` again which should provision
all kubernetes resources in your AKS cluster.

To destroy your AKS cluster use `terraform destroy`.

```shell
$ terraform destroy
```
