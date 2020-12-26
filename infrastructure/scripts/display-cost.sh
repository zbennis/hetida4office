#!/bin/sh
az consumption usage list --query "[?contains(usageStart, '2020-11')].{product:product, cost:join(' ', [pretaxCost, currency]), from:usageStart, to:usageEnd}" --output table
