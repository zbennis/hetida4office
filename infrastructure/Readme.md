# Setup for Azure
## Prerequsites
- Azure CLI installed
- Kubernetes CLI installed
- Terraform installed

## Install Kubernetes
After installing the Azure CLI and logging in. 

Use `terraform.tfvars` and dont check them in. 
Terraform will use these values to provision resources on Azure.

After you've done this, initalize your Terraform workspace, which will download 
the provider and initialize it with the values provided in the `terraform.tfvars` file.

```shell
$ terraform init
```


Then, provision your AKS cluster by running `terraform apply`. This will 
take approximately 10 minutes.

```shell
$ terraform apply
```

To destroy your AKS cluster use `terraform destroy`. 

```shell
$ terraform destroy
```

## Configure kubectl

To configure kubetcl run the following command:

```shell
$ az aks get-credentials --resource-group hetida4office-rg --name hetida4office-aks;
```


