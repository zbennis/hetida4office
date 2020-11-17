#!/bin/sh
az aks get-credentials --resource-group hetida4office-rg --name hetida4office-aks
kubectl delete clusterrolebinding kubernetes-dashboard
kubectl create clusterrolebinding kubernetes-dashboard --clusterrole=cluster-admin --serviceaccount=kube-system:kubernetes-dashboard --user=clusterUser
